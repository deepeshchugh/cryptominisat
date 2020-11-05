/******************************************
Copyright (C) 2009-2020 Authors of CryptoMiniSat, see AUTHORS file

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

#include "reducedb.h"
#include "solver.h"
#include "solverconf.h"
#include "sqlstats.h"
#ifdef FINAL_PREDICTOR
#include "cl_predictors.h"
#endif

// #define VERBOSE_DEBUG

#include <functional>
#include <cmath>

using namespace CMSat;

struct SortRedClsGlue
{
    explicit SortRedClsGlue(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->stats.glue < y->stats.glue;
    }
};

struct SortRedClsSize
{
    explicit SortRedClsSize(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->size() < y->size();
    }
};

struct SortRedClsAct
{
    explicit SortRedClsAct(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->stats.activity > y->stats.activity;
    }
};

#if defined(FINAL_PREDICTOR) || defined(STATS_NEEDED)
struct SortRedClsUIP1
{
    explicit SortRedClsUIP1(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->stats.uip1_used > y->stats.uip1_used;
    }
};

struct SortRedClsProps
{
    explicit SortRedClsProps(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->stats.props_made > y->stats.props_made;
    }
};
#endif

#ifdef FINAL_PREDICTOR
struct SortRedClsPredShort
{
    explicit SortRedClsPredShort(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->stats.pred_short_use > y->stats.pred_short_use;
    }
};

struct SortRedClsPredLong
{
    explicit SortRedClsPredLong(ClauseAllocator& _cl_alloc) :
        cl_alloc(_cl_alloc)
    {}
    ClauseAllocator& cl_alloc;

    bool operator () (const ClOffset xOff, const ClOffset yOff) const
    {
        const Clause* x = cl_alloc.ptr(xOff);
        const Clause* y = cl_alloc.ptr(yOff);
        return x->stats.pred_long_use > y->stats.pred_long_use;
    }
};

// struct SortRedClsPredForever
// {
//     explicit SortRedClsPredForever(ClauseAllocator& _cl_alloc) :
//         cl_alloc(_cl_alloc)
//     {}
//     ClauseAllocator& cl_alloc;
//
//     bool operator () (const ClOffset xOff, const ClOffset yOff) const
//     {
//         const Clause* x = cl_alloc.ptr(xOff);
//         const Clause* y = cl_alloc.ptr(yOff);
//         return x->stats.pred_forever_use > y->stats.pred_forever_use;
//     }
// };
#endif


ReduceDB::ReduceDB(Solver* _solver) :
    solver(_solver)
{
}

ReduceDB::~ReduceDB()
{
    #ifdef FINAL_PREDICTOR
    delete predictors;
    #endif
}

void ReduceDB::sort_red_cls(ClauseClean clean_type)
{
    #ifdef VERBOSE_DEBUG
    cout << "Before sort" << endl;
    uint64_t i = 0;
    for(const auto& x: solver->longRedCls[2]) {
        const ClOffset offset = x;
        Clause* cl = solver->cl_alloc.ptr(offset);
        cout << i << " offset: " << offset << " cl->stats.last_touched: " << cl->stats.last_touched
        << " act:" << std::setprecision(9) << cl->stats.activity
        << " which_red_array:" << cl->stats.which_red_array << endl
        << " -- cl:" << *cl << " tern:" << cl->is_ternary_resolvent
        << endl;
    }
    #endif

    switch (clean_type) {
        case ClauseClean::glue : {
            std::sort(solver->longRedCls[2].begin(), solver->longRedCls[2].end(), SortRedClsGlue(solver->cl_alloc));
            break;
        }

        case ClauseClean::activity : {
            std::sort(solver->longRedCls[2].begin(), solver->longRedCls[2].end(), SortRedClsAct(solver->cl_alloc));
            break;
        }

        default: {
            assert(false && "Unknown cleaning type");
        }
    }
}

//TODO maybe we chould count binary learnt clauses as well into the
//kept no. of clauses as other solvers do
void ReduceDB::handle_lev2()
{
    solver->dump_memory_stats_to_sql();
    size_t orig_size = solver->longRedCls[2].size();

    const double myTime = cpuTime();
    assert(solver->watches.get_smudged_list().empty());

    //lev2 -- clean
    int64_t num_to_reduce = solver->longRedCls[2].size();
    for(unsigned keep_type = 0
        ; keep_type < sizeof(solver->conf.ratio_keep_clauses)/sizeof(double)
        ; keep_type++
    ) {
        const uint64_t keep_num = (double)num_to_reduce*solver->conf.ratio_keep_clauses[keep_type];
        if (keep_num == 0) {
            continue;
        }
        sort_red_cls(static_cast<ClauseClean>(keep_type));
        mark_top_N_clauses(keep_num);
    }
    assert(delayed_clause_free.empty());
    cl_marked = 0;
    cl_ttl = 0;
    cl_locked_solver = 0;
    remove_cl_from_lev2();

    solver->clean_occur_from_removed_clauses_only_smudged();
    for(ClOffset offset: delayed_clause_free) {
        solver->free_cl(offset);
    }
    delayed_clause_free.clear();

    #ifdef SLOW_DEBUG
    solver->check_no_removed_or_freed_cl_in_watch();
    #endif

    if (solver->conf.verbosity >= 2) {
        cout << "c [DBclean lev2]"
        << " confl: " << solver->sumConflicts
        << " orig size: " << orig_size
        << " marked: " << cl_marked
        << " ttl:" << cl_ttl
        << " locked_solver:" << cl_locked_solver
        << solver->conf.print_times(cpuTime()-myTime)
        << endl;
    }

    if (solver->sqlStats) {
        solver->sqlStats->time_passed_min(
            solver
            , "dbclean-lev2"
            , cpuTime()-myTime
        );
    }
    total_time += cpuTime()-myTime;

    last_reducedb_num_conflicts = solver->sumConflicts;
}

#if defined(FINAL_PREDICTOR) || defined(STATS_NEEDED)
void ReduceDB::set_props_and_uip_ranks(vector<ClOffset>& all_learnt)
{
    std::sort(all_learnt.begin(), all_learnt.end(), SortRedClsProps(solver->cl_alloc));
    total_glue = 0;
    total_props = 0;
    total_uip1_used = 0;
    for(size_t i = 0; i < all_learnt.size(); i++) {
        ClOffset offs = all_learnt[i];
        Clause* cl = solver->cl_alloc.ptr(offs);
        cl->stats.props_made_rank = i+1;

        total_glue += cl->stats.glue;
        total_props += cl->stats.props_made;
        total_uip1_used += cl->stats.uip1_used;
    }
    median_props = solver->cl_alloc.ptr(all_learnt[all_learnt.size()/2])->stats.props_made;

    std::sort(all_learnt.begin(), all_learnt.end(), SortRedClsUIP1(solver->cl_alloc));
    for(size_t i = 0; i < all_learnt.size(); i++) {
        ClOffset offs = all_learnt[i];
        Clause* cl = solver->cl_alloc.ptr(offs);
        cl->stats.uip1_used_rank = i+1;
    }
    median_uip1_used = solver->cl_alloc.ptr(all_learnt[all_learnt.size()/2])->stats.uip1_used;
}
#endif

#ifdef STATS_NEEDED
void ReduceDB::dump_sql_cl_data(
    const uint32_t cur_rst_type
) {
    reduceDB_called++;
    double myTime = cpuTime();
    assert(solver->sqlStats);
    solver->sqlStats->begin_transaction();
    uint64_t added_to_db = 0;

    vector<ClOffset> all_learnt;
    for(uint32_t lev = 0; lev < solver->longRedCls.size(); lev++) {
        auto& cc = solver->longRedCls[lev];
        for(const auto& offs: cc) {
            Clause* cl = solver->cl_alloc.ptr(offs);
            assert(!cl->getRemoved());
            assert(cl->red());
            assert(!cl->freed());
            all_learnt.push_back(offs);
        }
    }
    if (all_learnt.empty()) {
        return;
    }

    set_props_and_uip_ranks(all_learnt);

    std::sort(all_learnt.begin(), all_learnt.end(), SortRedClsAct(solver->cl_alloc));
    float median_act = solver->cl_alloc.ptr(all_learnt[all_learnt.size()/2])->stats.activity;

    solver->sqlStats->reduceDB_common(
        solver,
        reduceDB_called,
        all_learnt.size(),
        cur_rst_type,
        median_act,
        median_uip1_used,
        median_props,
        (double)total_glue/(double)all_learnt.size(),
        (double)total_props/(double)all_learnt.size(),
        (double)total_uip1_used/(double)all_learnt.size()
    );

    for(size_t i = 0; i < all_learnt.size(); i++) {
        ClOffset offs = all_learnt[i];
        Clause* cl = solver->cl_alloc.ptr(offs);

        //Only if selected to be dumped
        if (cl->stats.ID == 0) {
            continue;
        }

        const bool locked = solver->clause_locked(*cl, offs);
        solver->sqlStats->reduceDB(
            solver
            , locked
            , cl
            , i+1
            , reduceDB_called
        );
        added_to_db++;
        cl->stats.dump_no++;
        cl->stats.reset_rdb_stats(solver->conf.rdb_discount_factor);
    }
    solver->sqlStats->end_transaction();

    if (solver->conf.verbosity) {
        cout << "c [sql] added to DB " << added_to_db
        << " dump-ratio: " << solver->conf.dump_individual_cldata_ratio
        << solver->conf.print_times(cpuTime()-myTime)
        << endl;
    }
}
#endif

void ReduceDB::handle_lev1()
{
    #ifdef VERBOSE_DEBUG
    cout << "c handle_lev1()" << endl;
    #endif

    uint32_t moved_w0 = 0;
    uint32_t used_recently = 0;
    uint32_t non_recent_use = 0;
    double myTime = cpuTime();
    size_t orig_size = solver->longRedCls[1].size();

    size_t j = 0;
    for(size_t i = 0
        ; i < solver->longRedCls[1].size()
        ; i++
    ) {
        const ClOffset offset = solver->longRedCls[1][i];
        Clause* cl = solver->cl_alloc.ptr(offset);
        #ifdef VERBOSE_DEBUG
        cout << "offset: " << offset << " cl->stats.last_touched: " << cl->stats.last_touched
        << " act:" << std::setprecision(9) << cl->stats.activity
        << " which_red_array:" << cl->stats.which_red_array << endl
        << " -- cl:" << *cl << " tern:" << cl->is_ternary_resolvent
        << endl;
        #endif

        assert(!cl->stats.locked_for_data_gen);
        if (cl->stats.which_red_array == 0) {
            solver->longRedCls[0].push_back(offset);
            moved_w0++;
        } else if (cl->stats.which_red_array == 2) {
            assert(false && "we should never move up through any other means");
        } else {
            uint32_t must_touch = solver->conf.must_touch_lev1_within;
            if (cl->is_ternary_resolvent) {
                must_touch *= solver->conf.ternary_keep_mult;
            }
            if (!solver->clause_locked(*cl, offset)
                && cl->stats.last_touched + must_touch < solver->sumConflicts
            ) {
                solver->longRedCls[2].push_back(offset);
                cl->stats.which_red_array = 2;

                //when stats are needed, activities are correctly updated
                //across all clauses
                //WARNING this changes the way things behave during STATS relative to non-STATS!
                #ifndef STATS_NEEDED
                cl->stats.activity = 0;
                solver->bump_cl_act<false>(cl);
                #endif
                non_recent_use++;
            } else {
                solver->longRedCls[1][j++] = offset;
                used_recently++;
            }
        }
    }
    solver->longRedCls[1].resize(j);

    if (solver->conf.verbosity >= 2) {
        cout << "c [DBclean lev1]"
        << " confl: " << solver->sumConflicts
        << " orig size: " << orig_size
        << " used recently: " << used_recently
        << " not used recently: " << non_recent_use
        << " moved w0: " << moved_w0
        << solver->conf.print_times(cpuTime()-myTime)
        << endl;
    }

    if (solver->sqlStats) {
        solver->sqlStats->time_passed_min(
            solver
            , "dbclean-lev1"
            , cpuTime()-myTime
        );
    }
    total_time += cpuTime()-myTime;
}

#ifdef FINAL_PREDICTOR
void ReduceDB::pred_move_to_lev1_and_lev0()
{
    // FOREVER
//     if (false) {
//         uint32_t marked_forever = 0;
//         uint32_t keep_forever = 300 * solver->conf.pred_forever_chunk_mult;
//         std::sort(solver->longRedCls[2].begin(), solver->longRedCls[2].end(),
//                   SortRedClsPredForever(solver->cl_alloc));
//         size_t j = 0;
//         for(uint32_t i = 0; i < solver->longRedCls[2].size(); i ++) {
//             const ClOffset offset = solver->longRedCls[2][i];
//             Clause* cl = solver->cl_alloc.ptr(offset);
//             if (i < keep_forever) {
//                 marked_forever++;
//                 cl->stats.which_red_array = 0;
//                 solver->longRedCls[0].push_back(offset);
//             } else {
//                 solver->longRedCls[2][j++] =solver->longRedCls[2][i];
//             }
//         }
//         solver->longRedCls[2].resize(j);
//     } else {
        uint32_t marked_forever = 0;
        size_t j = 0;
        for(uint32_t i = 0; i < solver->longRedCls[2].size(); i ++) {
            const ClOffset offset = solver->longRedCls[2][i];
            Clause* cl = solver->cl_alloc.ptr(offset);
            if (cl->stats.pred_forever_topperc < solver->conf.pred_forever_topperc) {
                marked_forever++;
                cl->stats.which_red_array = 0;
                solver->longRedCls[0].push_back(offset);
            } else {
                solver->longRedCls[2][j++] =solver->longRedCls[2][i];
            }
        }
        solver->longRedCls[2].resize(j);
//     }


    // LONG
    uint32_t marked_long = 0;
    uint32_t keep_long = 2000 * solver->conf.pred_long_chunk_mult;
    std::sort(solver->longRedCls[2].begin(), solver->longRedCls[2].end(),
              SortRedClsPredLong(solver->cl_alloc));

    j = 0;
    for(uint32_t i = 0; i < solver->longRedCls[2].size(); i ++) {
        const ClOffset offset = solver->longRedCls[2][i];
        Clause* cl = solver->cl_alloc.ptr(offset);
        if (i < keep_long) {
            marked_long++;
            cl->stats.which_red_array = 1;
            solver->longRedCls[1].push_back(offset);
        } else {
            solver->longRedCls[2][j++] =solver->longRedCls[2][i];
        }
    }
    solver->longRedCls[2].resize(j);
}

void ReduceDB::update_preds_lev2()
{
    double myTime = cpuTime();

    set_props_and_uip_ranks(solver->longRedCls[2]);

    std::sort(solver->longRedCls[2].begin(), solver->longRedCls[2].end(),
              SortRedClsAct(solver->cl_alloc));
    double size = (double)solver->longRedCls[2].size();
    double avg_props = (double)total_props/(double)size;
    for(size_t i = 0
        ; i < solver->longRedCls[2].size()
        ; i++
    ) {
        const ClOffset offset = solver->longRedCls[2][i];
        Clause* cl = solver->cl_alloc.ptr(offset);

        if (cl->stats.which_red_array == 0) {
            assert(false);
        }

        double act_ranking_rel = ((double)i+1.0)/size;
        double uip1_ranking_rel = (double)(cl->stats.uip1_used_rank+1)/size;
        double prop_ranking_rel = (double)(cl->stats.props_made_rank+1)/size;

        cl->stats.pred_short_use = 0;
        cl->stats.pred_long_use = 0;
        //cl->stats.pred_forever_use = 0;
        cl->stats.pred_forever_topperc = 100;
        if (cl->stats.dump_no > 0) {
            predictors->predict(
                cl,
                solver->sumConflicts,
                act_ranking_rel,
                uip1_ranking_rel,
                prop_ranking_rel,
                avg_props,
                cl->stats.pred_short_use,
                cl->stats.pred_long_use,
                cl->stats.pred_forever_topperc
            );
        }
    }

    if (solver->conf.verbosity >= 2) {
        double predTime = cpuTime() - myTime;
        cout << "c [DBCL] main predtime: " << predTime << endl;
    }
}

void ReduceDB::clean_lev0_once_in_a_while(uint32_t& deleted)
{
    //Deal with FOREVER once in a while
    if (num_times_pred_called % solver->conf.pred_forever_check_every_n ==
        (solver->conf.pred_forever_check_every_n-1)
    ) {
        //Recalc pred_forever_use
        set_props_and_uip_ranks(solver->longRedCls[0]);
        double size = (double)solver->longRedCls[0].size();
        double avg_props = (double)total_props/(double)size;

        std::sort(solver->longRedCls[0].begin(), solver->longRedCls[0].end(),
              SortRedClsAct(solver->cl_alloc));

        for(size_t i = 0
            ; i < solver->longRedCls[0].size()
            ; i++
        ) {
            const ClOffset offset = solver->longRedCls[0][i];
            Clause* cl = solver->cl_alloc.ptr(offset);

            double act_ranking_rel = ((double)i+1.0)/size;
            double uip1_ranking_rel = (double)(cl->stats.uip1_used_rank+1)/size;
            double prop_ranking_rel = (double)(cl->stats.props_made_rank+1)/size;

            cl->stats.pred_forever_topperc = predictors->predict(
                predict_type::forever_pred,
                cl,
                solver->sumConflicts,
                act_ranking_rel,
                uip1_ranking_rel,
                prop_ranking_rel,
                avg_props
            );
        }

        //Clean up FOREVER, move to LONG
        const uint32_t checked_every = solver->conf.pred_forever_check_every_n * 10000;
//         if (false) {
//             const uint32_t orig_keep_forever = 2000.0*std::sqrt((double)solver->sumConflicts/10000.0) *
//         (double)solver->conf.pred_forever_size_mult;
//             uint32_t keep_forever = orig_keep_forever;
//             std::sort(solver->longRedCls[0].begin(), solver->longRedCls[0].end(),
//                   SortRedClsPredForever(solver->cl_alloc));
//             uint32_t j = 0;
//             for(uint32_t i = 0; i < solver->longRedCls[0].size(); i ++) {
//                 const ClOffset offset = solver->longRedCls[0][i];
//                 Clause* cl = solver->cl_alloc.ptr(offset);
//                 uint32_t time_inside_solver = solver->sumConflicts - cl->stats.introduced_at_conflict;
//
//                 if (i < keep_forever || time_inside_solver < checked_every) {
//                     assert(cl->stats.which_red_array == 0);
//                     if (time_inside_solver < checked_every) {
//                         keep_forever++;
//                     }
//                     solver->longRedCls[0][j++] =solver->longRedCls[0][i];
//                 } else {
//                     solver->longRedCls[1].push_back(offset);
//                     cl->stats.which_red_array = 1;
//                 }
//             }
//             solver->longRedCls[0].resize(j);
//         } else {
            uint32_t j = 0;
            for(uint32_t i = 0; i < solver->longRedCls[0].size(); i ++) {
                const ClOffset offset = solver->longRedCls[0][i];
                Clause* cl = solver->cl_alloc.ptr(offset);
                assert(!cl->freed());

                uint32_t time_inside_solver = solver->sumConflicts - cl->stats.introduced_at_conflict;
                if (cl->stats.pred_forever_topperc < solver->conf.pred_forever_topperc ||
                    solver->clause_locked(*cl, offset) ||
                    time_inside_solver < checked_every
                ) {
                    assert(cl->stats.which_red_array == 0);
                    solver->longRedCls[0][j++] = solver->longRedCls[0][i];
                } else {
//                     solver->longRedCls[1].push_back(offset);
//                     cl->stats.which_red_array = 1;
                    deleted++;
                    solver->watches.smudge((*cl)[0]);
                    solver->watches.smudge((*cl)[1]);
                    solver->litStats.redLits -= cl->size();

                    *solver->drat << del << *cl << fin;
                    cl->setRemoved();
                    delayed_clause_free.push_back(offset);
                }
            }
            solver->longRedCls[0].resize(j);
//         }
    }
}

void ReduceDB::clean_lev1_once_in_a_while(uint32_t& deleted)
{
    //Deal with LONG once in a while
    if (num_times_pred_called % solver->conf.pred_long_check_every_n ==
        (solver->conf.pred_long_check_every_n-1)
    ) {
        //Recalc pred_long_use
        set_props_and_uip_ranks(solver->longRedCls[1]);
        double size = (double)solver->longRedCls[1].size();
        double avg_props = (double)total_props/(double)size;

        std::sort(solver->longRedCls[1].begin(), solver->longRedCls[1].end(),
              SortRedClsAct(solver->cl_alloc));

        for(size_t i = 0
            ; i < solver->longRedCls[1].size()
            ; i++
        ) {
            const ClOffset offset = solver->longRedCls[1][i];
            Clause* cl = solver->cl_alloc.ptr(offset);
            double act_ranking_rel = ((double)i+1.0)/size;
            double uip1_ranking_rel = (double)(cl->stats.uip1_used_rank+1)/size;
            double prop_ranking_rel = (double)(cl->stats.props_made_rank+1)/size;

            cl->stats.pred_long_use = predictors->predict(
                predict_type::long_pred,
                cl,
                solver->sumConflicts,
                act_ranking_rel,
                uip1_ranking_rel,
                prop_ranking_rel,
                avg_props
            );
        }

        //Clean up LONG, move to SHORT
        const uint32_t checked_every = solver->conf.pred_long_check_every_n * 10000;
        std::sort(solver->longRedCls[1].begin(), solver->longRedCls[1].end(),
              SortRedClsPredLong(solver->cl_alloc));
        uint32_t keep_long = 15000 * solver->conf.pred_long_size_mult;
        uint32_t j = 0;
        for(uint32_t i = 0; i < solver->longRedCls[1].size(); i ++) {
            const ClOffset offset = solver->longRedCls[1][i];
            Clause* cl = solver->cl_alloc.ptr(offset);
            assert(!cl->freed());

            uint32_t time_inside_solver = solver->sumConflicts - cl->stats.introduced_at_conflict;
            if (i < keep_long ||
                solver->clause_locked(*cl, offset) ||
                time_inside_solver < checked_every)
            {
                assert(cl->stats.which_red_array == 1);
                if (time_inside_solver < checked_every ||
                    solver->clause_locked(*cl, offset))
                {
                    keep_long++;
                }
                solver->longRedCls[1][j++] =solver->longRedCls[1][i];
            } else {
//                 solver->longRedCls[2].push_back(offset);
//                 cl->stats.which_red_array = 2;
                deleted++;
                solver->watches.smudge((*cl)[0]);
                solver->watches.smudge((*cl)[1]);
                solver->litStats.redLits -= cl->size();

                *solver->drat << del << *cl << fin;
                cl->setRemoved();
                delayed_clause_free.push_back(offset);
            }
        }
        solver->longRedCls[1].resize(j);
    }
}

void ReduceDB::delete_from_lev2(uint32_t& deleted)
{
    // SHORT
    uint32_t keep_short = 15000 * solver->conf.pred_short_size_mult;
    std::sort(solver->longRedCls[2].begin(), solver->longRedCls[2].end(),
              SortRedClsPredShort(solver->cl_alloc));

    uint32_t j = 0;
    for(uint32_t i = 0; i < solver->longRedCls[2].size(); i ++) {
        const ClOffset offset = solver->longRedCls[2][i];
        Clause* cl = solver->cl_alloc.ptr(offset);
        assert(!cl->freed());

        if (i < keep_short
            || solver->clause_locked(*cl, offset)
            || cl->stats.dump_no == 0
        ) {
            if (solver->clause_locked(*cl, offset)
                || cl->stats.dump_no == 0)
            {
                keep_short++;
            }
            solver->longRedCls[2][j++] =solver->longRedCls[2][i];
        } else {
            deleted++;
            solver->watches.smudge((*cl)[0]);
            solver->watches.smudge((*cl)[1]);
            solver->litStats.redLits -= cl->size();

            *solver->drat << del << *cl << fin;
            cl->setRemoved();
            delayed_clause_free.push_back(offset);
        }
    }
    solver->longRedCls[2].resize(j);
}

void ReduceDB::reset_clause_dats(const uint32_t lev)
{
    uint32_t tot_dumpno = 0;
    for(const auto& off: solver->longRedCls[lev]) {
        Clause* cl = solver->cl_alloc.ptr(off);
        assert(!cl->freed());

        tot_dumpno += cl->stats.dump_no;
        cl->stats.dump_no++;
        cl->stats.reset_rdb_stats(solver->conf.rdb_discount_factor);
    }

    if (solver->conf.verbosity) {
        cout << "c [DBCL pred]"
        << " lev: " << lev
        << " avg dumpno: " << std::fixed << std::setprecision(2)
        << ratio_for_stat(tot_dumpno, solver->longRedCls[lev].size())
        << endl;
    }
}

void ReduceDB::handle_lev2_predictor()
{
    num_times_pred_called++;
    if (predictors == NULL) {
        predictors = new ClPredictors;
        predictors->load_models(
            solver->conf.pred_conf_short,
            solver->conf.pred_conf_long,
            solver->conf.pred_conf_forever);
    }

    assert(delayed_clause_free.empty());
    uint32_t deleted = 0;
    double myTime = cpuTime();

    update_preds_lev2();
    pred_move_to_lev1_and_lev0();
    delete_from_lev2(deleted);
    clean_lev0_once_in_a_while(deleted);
    clean_lev1_once_in_a_while(deleted);
    reset_clause_dats(0);
    reset_clause_dats(1);
    reset_clause_dats(2);

    //Cleanup
    solver->clean_occur_from_removed_clauses_only_smudged();
    for(ClOffset offset: delayed_clause_free) {
        solver->free_cl(offset);
    }
    delayed_clause_free.clear();

    //Stats
    if (solver->conf.verbosity >= 1) {
        cout
        << "c [DBCL pred]"
        << " del: "    << print_value_kilo_mega(deleted)
        << " short: " << print_value_kilo_mega(solver->longRedCls[2].size())
        << " long: "  << print_value_kilo_mega(solver->longRedCls[1].size())
        << " forever: "  << print_value_kilo_mega(solver->longRedCls[0].size())
        << endl;

        if (solver->conf.verbosity >= 2) {
        cout
        << "c [DBCL pred] lev0: " << solver->longRedCls[0].size() << endl
        << "c [DBCL pred] lev1: " << solver->longRedCls[1].size() << endl
        << "c [DBCL pred] lev2: " << solver->longRedCls[2].size() << endl;
        }

        cout
        << solver->conf.print_times(cpuTime()-myTime)
        << endl;
    }

    if (solver->sqlStats) {
        solver->sqlStats->time_passed_min(
            solver
            , "dbclean-lev1"
            , cpuTime()-myTime
        );
    }
    total_time += cpuTime()-myTime;
}
#endif

void ReduceDB::mark_top_N_clauses(const uint64_t keep_num)
{
    #ifdef VERBOSE_DEBUG
    cout << "Marking top N clauses " << keep_num << endl;
    #endif

    size_t marked = 0;
    for(size_t i = 0
        ; i < solver->longRedCls[2].size() && marked < keep_num
        ; i++
    ) {
        const ClOffset offset = solver->longRedCls[2][i];
        Clause* cl = solver->cl_alloc.ptr(offset);
        #ifdef VERBOSE_DEBUG
        cout << "offset: " << offset << " cl->stats.last_touched: " << cl->stats.last_touched
        << " act:" << std::setprecision(9) << cl->stats.activity
        << " which_red_array:" << cl->stats.which_red_array << endl
        << " -- cl:" << *cl << " tern:" << cl->is_ternary_resolvent
        << endl;
        #endif

        if (cl->used_in_xor()
            || cl->stats.ttl > 0
            || solver->clause_locked(*cl, offset)
            || cl->stats.which_red_array != 2
        ) {
            //no need to mark, skip
            #ifdef VERBOSE_DEBUG
            cout << "Not marking Skipping "<< endl;
            #endif
            continue;
        }

        if (!cl->stats.marked_clause) {
            marked++;
            cl->stats.marked_clause = true;
            #ifdef VERBOSE_DEBUG
            cout << "Not marking Skipping "<< endl;
            #endif
        }
    }
}

bool ReduceDB::cl_needs_removal(const Clause* cl, const ClOffset offset) const
{
    assert(cl->red());
    return !cl->used_in_xor()
         && !cl->stats.marked_clause
         && cl->stats.ttl == 0
         && !solver->clause_locked(*cl, offset);
}

void ReduceDB::remove_cl_from_lev2() {
    size_t i, j;
    for (i = j = 0
        ; i < solver->longRedCls[2].size()
        ; i++
    ) {
        ClOffset offset = solver->longRedCls[2][i];
        Clause* cl = solver->cl_alloc.ptr(offset);
        assert(cl->size() > 2);

        //move to another array
        if (cl->stats.which_red_array < 2) {
            cl->stats.marked_clause = 0;
            solver->longRedCls[cl->stats.which_red_array].push_back(offset);
            continue;
        }
        assert(cl->stats.which_red_array == 2);

        //Check if locked, or marked or ttl-ed
        if (cl->stats.marked_clause) {
            cl_marked++;
        } else if (cl->stats.ttl != 0) {
            cl_ttl++;
        } else if (solver->clause_locked(*cl, offset)) {
            cl_locked_solver++;
        }

        if (!cl_needs_removal(cl, offset)) {
            if (cl->stats.ttl > 0) {
                cl->stats.ttl--;
            }
            solver->longRedCls[2][j++] = offset;
            cl->stats.marked_clause = 0;
            continue;
        }

        //Stats Update
        solver->watches.smudge((*cl)[0]);
        solver->watches.smudge((*cl)[1]);
        solver->litStats.redLits -= cl->size();

        *solver->drat << del << *cl << fin;
        cl->setRemoved();
        #ifdef VERBOSE_DEBUG
        cout << "REMOVING offset: " << offset << " cl->stats.last_touched: " << cl->stats.last_touched
        << " act:" << std::setprecision(9) << cl->stats.activity
        << " which_red_array:" << cl->stats.which_red_array << endl
        << " -- cl:" << *cl << " tern:" << cl->is_ternary_resolvent
        << endl;
        #endif
        delayed_clause_free.push_back(offset);
    }
    solver->longRedCls[2].resize(j);
}
