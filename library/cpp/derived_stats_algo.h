/*
 * Copyright (c) 2016 Juniper Networks, Inc. All rights reserved.
 */


// derived_stats_algo.h
// 
// This file has the implementation of 
// derived stats algorithms
//
#ifndef __DERIVED_STATS_ALGO_H__
#define __DERIVED_STATS_ALGO_H__

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <sandesh/derived_stats_results_types.h>

using std::vector;
using std::map;
using std::make_pair;
using std::string;

namespace contrail {
namespace sandesh {

template<class CatElemT, class CatResT>
class DSCategoryCount {
};

template<>
class DSCategoryCount<CategoryResult,CategoryResult> {
  public: 
    DSCategoryCount(const std::string &annotation) {}

    typedef map<std::string, uint64_t> CatResT;
    CatResT agg_counts_;
    CatResT diff_counts_;

    bool FillResult(CategoryResult &res) const {
        res.set_counters(diff_counts_);
        return !diff_counts_.empty();
    }

    void Update(const CategoryResult& raw) {
        diff_counts_.clear();
        for (CatResT::const_iterator it = raw.get_counters().begin();
                it != raw.get_counters().end(); it++) {

            // Is there anything to count?
            if (!it->second) continue;
          
            CatResT::iterator mit =
                    agg_counts_.find(it->first);
 
            if (mit==agg_counts_.end()) {
                // This is a new category
                diff_counts_.insert(make_pair(it->first, it->second));
                agg_counts_.insert(make_pair(it->first, it->second));
            } else {
                assert(it->second >= mit->second);
                uint64_t diff = it->second - mit->second;
                if (diff) {
                    // If the count for this category has changed,
                    // report the diff and update the aggregate
                    diff_counts_.insert(make_pair(it->first, diff));
                    mit->second = it->second;
                }
            }
        }
    }
};


template <typename ElemT, class EWMResT>
class DSEWM {
  public:
    DSEWM(const std::string &annotation):
                mean_(0), variance_(0), samples_(0)  { 
        alpha_ = (double) strtod(annotation.c_str(), NULL);
        assert(alpha_ > 0);
        assert(alpha_ < 1);
    }
    double alpha_;
    double mean_;
    double variance_;
    double sigma_;
    double stddev_;
    uint64_t samples_;

    bool FillResult(EWMResT &res) const {
        res.set_samples(samples_);
        res.set_mean(mean_);
        res.set_stddev(stddev_);
        res.set_sigma(sigma_);
        return true;
    }
    void Update(const ElemT& raw) {
        samples_++;
        variance_ = (1-alpha_)*(variance_ + (alpha_*pow(raw-mean_,2)));
        mean_ = ((1-alpha_)*mean_) + (alpha_*raw);
        stddev_ = sqrt(variance_);
        if (stddev_) sigma_ = (raw - mean_) / stddev_;
        else sigma_ = 0;
    }
};

template <typename ElemT, class NullResT>
class DSNull {
  public:
    DSNull(const std::string &annotation): samples_(0) {}
    ElemT value_;
    uint64_t samples_;

    bool FillResult(NullResT &res) const {
        res.set_samples(samples_);
        res.set_value(value_);
        return true;
    }
    void Update(const ElemT& raw) {
        samples_++;
        value_ = raw;
    }
};

template <typename ElemT, class DiffResT>
class DSDiff {
  public:
    DSDiff(const std::string &annotation) : init_(false) {}
    bool init_;
    ElemT agg_;
    ElemT diff_;

    bool FillResult(DiffResT &res) const {
        ElemT empty;
        if (!init_) return false;
        if (diff_ == empty) return false;
        res = diff_;
        return true;
    }
    void Update(const ElemT& raw) {
        if (!init_) {
            diff_ = raw;
            agg_ = raw;
        } else {
            diff_ = raw - agg_;
            agg_ = agg_ + diff_;
        }
        init_ = true;
    }
};

template <typename ElemT, class SumResT> 
class DSSum {
  public:
    DSSum(const std::string &annotation): samples_(0) {
        if (annotation.empty()) {
            history_count_ = 0;
        } else {
            history_count_ = (uint64_t) strtoul(annotation.c_str(), NULL, 10);
        }
    }
    uint64_t samples_;
    SumResT value_;
    vector<ElemT> history_buf_;
    uint64_t history_count_;

    virtual bool FillResult(SumResT &res) const {
        if (!samples_) return false;
        res = value_;
        return true;
    }

    virtual void Update(const ElemT& raw) {
        if (!samples_) {
            value_ = raw;
        } else {
            value_ = value_ + raw;
        }
        if (history_count_) {
            // if 'history_count_' is non-0, we need to aggregate only
            // the last 'history_count_' elements
            if (samples_ >= history_count_) {
                // We must substract the oldest entry, if it exists
                value_ = value_ - history_buf_[samples_ % history_count_];
                // Now record the latest update, so we can subtract when needed
                history_buf_[samples_ % history_count_] = raw;
            } else {
                history_buf_.push_back(raw);
            }
        }
        samples_++;
    }
};

template <typename ElemT, class AvgResT>
class DSAvg : public DSSum<ElemT,AvgResT> {
  public:
    DSAvg(const std::string &annotation): DSSum<ElemT,AvgResT>(annotation) {}

    virtual bool FillResult(AvgResT &res) const {
        if (!DSSum<ElemT,AvgResT>::samples_) return false;
        if (DSSum<ElemT,AvgResT>::history_count_) {
	    uint64_t div =
                    ((DSSum<ElemT,AvgResT>::samples_ >=DSSum<ElemT,AvgResT>::history_count_) ?
		     DSSum<ElemT,AvgResT>::history_count_ : DSSum<ElemT,AvgResT>::samples_);
	    res = DSSum<ElemT,AvgResT>::value_ / div;
        } else {
            res = DSSum<ElemT,AvgResT>::value_ / DSSum<ElemT,AvgResT>::samples_;
        }
        return true;
    }
};

} // namespace sandesh
} // namespace contrail

#endif // #define __DERIVED_STATS_ALGO_H__
