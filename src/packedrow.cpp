/******************************************
Copyright (c) 2018  Mate Soos
Copyright (c) 2012  Cheng-Shen Han
Copyright (c) 2012  Jie-Hong Roland Jiang

For more information, see " When Boolean Satisfiability Meets Gaussian
Elimination in a Simplex Way." by Cheng-Shen Han and Jie-Hong Roland Jiang
in CAV (Computer Aided Verification), 2012: 410-426


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
***********************************************/

#include "packedrow.h"

using namespace CMSat;

bool PackedRow::fill(
    vec<Lit>& tmp_clause,
    const vec<lbool>& assigns,
    const vector<uint32_t>& col_to_var_original
) const
{
    bool final = !rhs_internal;

    tmp_clause.clear();
    uint32_t col = 0;
    bool wasundef = false;
    for (uint32_t i = 0; i < size; i++) for (uint32_t i2 = 0; i2 < 64; i2++) {
        if ((mp[i] >> i2) &1) {
            const uint32_t& var = col_to_var_original[col];
            assert(var != std::numeric_limits<uint32_t>::max());

            const lbool val = assigns[var];
            const bool val_bool = val == l_True;
            tmp_clause.push(Lit(var, val_bool));
            final ^= val_bool;
            if (val == l_Undef) {
                assert(!wasundef);
                Lit tmp(tmp_clause[0]);
                tmp_clause[0] = tmp_clause.last();
                tmp_clause.last() = tmp;
                wasundef = true;
            }
        }
        col++;
    }
    if (wasundef) {
        tmp_clause[0] ^= final;
        //assert(ps != ps_first+1);
    } else
        assert(!final);

    return wasundef;
}

///returns popcnt
uint32_t PackedRow::find_watchVar(
    vector<Lit>& tmp_clause,
    const vector<uint32_t>& col_to_var,
    vec<bool> &is_basic,
    uint32_t& nb_var
) {
    uint32_t  tmp_var = 0;
    uint32_t popcnt = 0;
    nb_var = std::numeric_limits<uint32_t>::max();
    uint32_t i;
    tmp_clause.clear();

    for(i = 0; i < size*64 && popcnt < 3; i++) {
        if (this->operator[](i)){
            popcnt++;
            tmp_var = col_to_var[i];
            tmp_clause.push_back(Lit(tmp_var, false));
            if( !is_basic[tmp_var]){  //nobasic
                nb_var = tmp_var;
            }else{  // basic
                Lit tmp(tmp_clause[0]);
                tmp_clause[0] = tmp_clause.back();
                tmp_clause.back() = tmp;
            }
        }
    }
    assert(tmp_clause.size() == popcnt);
    assert( popcnt == 0 || is_basic[ tmp_clause[0].var() ]) ;
    return popcnt;
}

gret PackedRow::propGause(
    vector<Lit>& tmp_clause,
    const vector<lbool>& assigns,
    const vector<uint32_t>& col_to_var,
    vec<bool> &is_basic,
    uint32_t& nb_var,
    uint32_t start_col
) {
    bool final = !rhs_internal;
    nb_var = std::numeric_limits<uint32_t>::max();
    tmp_clause.clear();

    for (uint32_t i = start_col; i != size; i++) if (mp[i]) {
        uint64_t tmp = mp[i];
        uint32_t at = i*64;
        for (uint32_t i2 = 0 ; i2 < 64; i2++) {
            if(tmp & 1){
                const uint32_t var = col_to_var[at  + i2];
                const lbool val = assigns[var];

                //TODO: let's put the most UNDEF variables
                //TODO: at the beginning of the matrix

                // found new non-basic variable, let's watch it
                //TODO understand why is !is_basic[var] here?? whaaat? if it's UNDEF how would it propagate?
                if (val == l_Undef && !is_basic[var]) {
                    nb_var = var;
                    return gret::nothing_fnewwatch;
                }
                const bool val_bool = (val == l_True);
                final ^= val_bool;
                tmp_clause.push_back(Lit(var, val_bool));

                //if this is the basic variable, put it to the 0th position
                if (is_basic[var]) {
                    std::swap(tmp_clause[0], tmp_clause.back());
                }
            }
            tmp >>= 1;
        }
    }

    #ifdef SLOW_DEBUG
    {
        for (uint32_t i = 0; i != size; i++) if (mp[i]) {
            uint64_t tmp = mp[i];
            uint32_t at = i*64;
            for (uint32_t i2 = 0 ; i2 < 64; i2++) {
                if(tmp & 1){
                    const uint32_t var = col_to_var[at  + i2];
                    const lbool val = assigns[var];
                    if (val == l_Undef && !is_basic[var]) {
                        assert(false);
                    }
                }
                tmp >>= 1;
            }
        }
    }
    #endif

    if (assigns[tmp_clause[0].var()] == l_Undef) {
        tmp_clause[0] = tmp_clause[0].unsign()^final;
        return gret::prop;
    } else if (!final) {
        return gret::confl;
    }
    // this row is already satisfied, all variables are set
    return gret::nothing;

}




