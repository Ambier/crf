#include "base.h"

#include "prob.h"
#include "hashtable/size.h"
#include "tagset.h"
#include "taglimits.h"
#include "factor/factor.h"
#include "factor/variable.h"
#include "factor/message_map.h"
#include "factor/factor_graph.h"

namespace NLP {
  FactorGraph::FactorGraph(TagLimits &limits) :
    pool(new Util::Pool(1 << 18)), limits(limits), variables(), factors(),
    randomized_factors(), messages() { }

  void FactorGraph::_build_variables(size_t max_size) {
    for (size_t i = 0; i < limits.ntypes(); ++i) {
      size_t tag_offset = limits[i].prev.id();
      for (size_t j = 0; j < max_size; ++j) {
        size_t nfactors = limits.ntypes() - 1;
        if (j != 0)
          ++nfactors;
        if (j != max_size - 1)
          ++nfactors;
        Variable *v = new (pool, nfactors) Variable(j, limits.real_ntags(i), tag_offset, 0);
        variables.push_back(v);
      }
    }
  }

  void FactorGraph::_build_factors(size_t max_size) {
    for (size_t i = 0; i < variables.size(); ++i) {
      // factor from variable to the next time slice
      size_t index = i % max_size;
      if (i % max_size != max_size - 1) {
        Factor *f = new (pool, 2) Factor(index + 1, 2, false);
        f->variables[0] = variables[i];
        f->variables[1] = variables[i + 1];
        variables[i]->factors[(variables[i]->nfactors)++] = f;
        variables[i+1]->factors[(variables[i+1]->nfactors)++] = f;
        factors.push_back(f);
        //std::cout << "created factor between " << i << " " << i + 1 << std::endl;
      }
      // factor between variables at the same time slice
      size_t j = i;
      while (j < variables.size() - max_size) {
        Factor *f = new (pool, 2) Factor(index, 2, true);
        f->variables[0] = variables[j];
        f->variables[1] = variables[j + max_size];
        factors.push_back(f);
        //std::cout << "created factor between " << j << " " << j + max_size << std::endl;
        j += max_size;
      }
    }
  }

  bool FactorGraph::converged(double *old_messages, double *new_messages, size_t nmessages, const double threshold) {
    bool converged = true;
    for (size_t i = 0; i < nmessages; ++i) {
      converged = (std::fabs(old_messages[i] - new_messages[i]) < threshold) && converged;
      std::cout << "convergence " << old_messages[i] << ' ' << new_messages[i] << ' ' << (std::fabs(old_messages[i] - new_messages[i]) < threshold) << ' ' << converged << std::endl ;

    }
    return converged;
  }

  bool FactorGraph::message_from_variable(PSIs &psis, Variable *from, Factor *to, const double threshold) {
    //std::cout << "message_from_variable" << std::endl;
    double *old_messages = new double[from->ntags];
    double *msgs = messages(from, to);
    bool ret = msgs && memcpy(old_messages, msgs, sizeof(double) * from->ntags);

    for (size_t i = 0; i < from->ntags; ++i) {
      Tag tag = i + from->tag_offset;
      double msg = psis[from->index][None::val][tag];
      for (size_t j = 0; j < from->nfactors; ++j) {
        Factor *in = from->factors[j];
        if (in != to) {
          //if (std::isnan(messages(in, from, tag)))
            //std::cout << "NAN: " << from->index << ' ' << tag << std::endl;
          //else if (isinf(messages(in, from, tag)))
            //std::cout << "INF: " << from->index << ' ' << tag << std::endl;
          msg *= messages(in, from, tag);
          //if (std::isnan(msg))
            //std::cout << "NAN: " << from->index << ' ' << tag << ' ' << messages(in, from, tag) << std::endl;
          //else if (isinf(msg))
            //std::cout << "INF: " << from->index << ' ' << tag << ' ' << messages(in, from, tag) << std::endl;
        }
      }
      messages(from, to, i, msg);
    }
    ret = ret && converged(old_messages, messages(from, to), from->ntags, threshold);
    delete [] old_messages;
    return ret;
  }

  bool FactorGraph::message_from_factor(PSIs &psis, Factor *from, Variable *to, const double threshold) {
    //std::cout << "message_from_factor" << std::endl;
    double sum = 0.0;
    double *old_messages = new double[to->ntags];
    double *msgs = messages(from, to);
    bool ret = msgs && memcpy(old_messages, msgs, sizeof(double) * to->ntags);

    for (size_t i = 0; i < to->ntags; ++i) {
      Tag t1 = i + to->tag_offset;
      double msg = 0.0;
      for (size_t j = 0; j < from->nvars; ++j) {
        Variable *in = from->variables[j];
        if (in != to) {
          for (size_t k = 0; k < in->ntags; ++k) {
            Tag t2 = k + in->tag_offset;
            //std::cout << from->index << ' ' << t1 << ' ' << t2 << ' ' << psis[from->index][(j == 0) ? t1 : t2][(j == 0) ? t2 : t1] << ' ' << messages(in, from, k) << std::endl;
            msg += psis[from->index][(j == 0) ? t1 : t2][(j == 0) ? t2 : t1] * messages(in, from, k);
          }
        }
      }
      sum += msg;
      messages(from, to, i, msg);
    }
    double norm = (sum != 0) ? 1.0 / sum : 1.0;
    messages.normalize(from, to, norm);
    from->norm = norm;
    ret = ret && converged(old_messages, messages(from, to), to->ntags, threshold);
    delete [] old_messages;
    return ret;
  }

  bool FactorGraph::propagate(PSIs &psis, size_t max_size, size_t max_iterations, const double threshold) {
    bool converged = true;
    messages.reset();
    randomized_factors.clear();
    size_t limit = (max_size - 1) * limits.ntypes();
    std::copy(factors.begin(), factors.begin() + limit, std::back_inserter(randomized_factors));

    for (size_t iter = 0; iter < max_iterations; ++iter) {
      converged = true;
      std::cout << "Iteration " << iter << std::endl;
      std::random_shuffle(randomized_factors.begin(), randomized_factors.end());
      for (Factors::iterator i = randomized_factors.begin(); i != randomized_factors.end(); ++i) {
        Factor *to = *i;
        for (size_t j = 0; j < to->nvars; ++j) {
          Variable *from = to->variables[j];
          converged = message_from_variable(psis, from, to, threshold) && converged;
        }
      }
      for (Factors::iterator i = randomized_factors.begin(); i != randomized_factors.end(); ++i) {
        Factor *from = *i;
        for (size_t j = 0; j < from->nvars; ++j) {
          Variable *to = from->variables[j];
          converged = message_from_factor(psis, from, to, threshold) && converged;
        }
      }
      if (converged) {
        std::cout << "converged: true after " << iter << " iterations" << std::endl;
        break;
      }
    }
    if (!converged)
      std::cout << "converged: false" << std::endl;
    return converged;
  }

  double FactorGraph::_compute_state_marginals(PSIs &psis, PDFs &state_marginals, Variable *variable, size_t index) {
    double norm = 0.0;
    double log_partition = 0.0;
    size_t state_max = variable->tag_offset + variable->ntags;

    for (size_t state = 0; state < variable->ntags; ++state) {
      Tag tag = state + variable->tag_offset;
      double msg_product = messages(variable->factors[0], variable, state) * messages(variable->factors[1], variable, state);
      double belief = psis[index][None::val][tag] * msg_product;
      norm += belief;
      log_partition += msg_product;
      state_marginals[index][tag] = belief;
    }
    norm = (norm != 0) ? 1.0 / norm : 1.0;

    for (size_t state = variable->tag_offset; state < state_max; ++state)
      state_marginals[index][state] *= norm;

    return std::log(log_partition);
  }

  double FactorGraph::_compute_trans_marginals(PSIs &psis, PDFs &trans_marginals, Factor *factor, Variable *from, Variable *to) {
    double norm = 0.0;
    size_t prev_max = from->tag_offset + from->ntags;
    size_t curr_max = to->tag_offset + to->ntags;
    for (size_t prev = 0; prev < from->ntags; ++prev) {
      for (size_t curr = 0; curr < to->ntags; ++curr) {
        Tag prev_tag = prev + from->tag_offset;
        Tag curr_tag = curr + to->tag_offset;
        double belief = psis[factor->index][prev_tag][curr_tag] * messages(from, factor, prev) * messages(to, factor, curr);
        norm += belief;
        trans_marginals[prev_tag][curr_tag] = belief;
      }
    }
    //factor->norm = norm; //is this needed?
    norm = (norm != 0) ? 1.0 / norm : 1.0;

    for (size_t prev_tag = from->tag_offset; prev_tag < prev_max; ++prev_tag)
      for (size_t curr_tag = to->tag_offset; curr_tag < curr_max; ++curr_tag)
        trans_marginals[prev_tag][curr_tag] *= norm;

    return std::log(factor->norm);
  }

  double FactorGraph::marginals(PSIs &psis, PDFs &state_marginals, PDFs &trans_marginals) {
    double log_partition = 0.0;
    for (Factors::iterator i = randomized_factors.begin(); i != randomized_factors.end(); ++i) {
      Factor *factor = *i;
      Variable *from = factor->variables[0];
      Variable *to = factor->variables[1];

      if (!factor->between_chains) {
        if (factor->index == 1)
          log_partition += _compute_state_marginals(psis, state_marginals, from, 0);
        log_partition += _compute_state_marginals(psis, state_marginals, to, factor->index);
      }
      log_partition += _compute_trans_marginals(psis, trans_marginals, factor, from, to);
    }

    return log_partition;
  }
}
