/**
 *  MIT License
 *
 *  Copyright (c) 2019 Yuya Kudo
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <Planner/RRTStar/RRTStar.h>

namespace planner {
    RRTStar::RRTStar(const uint32_t& dim,
                     const uint32_t& max_sampling_num,
                     const double&   goal_sampling_rate,
                     const double&   expand_dist,
                     const double&   R) :
        base::PlannerBase(dim, std::make_shared<KDTreeNodeList>(dim)),
        max_sampling_num_(max_sampling_num),
        expand_dist_(expand_dist),
        R_(R) {
        setGoalSamplingRate(goal_sampling_rate);
    }

    RRTStar::~RRTStar() {
    }

    void RRTStar::setMaxSamplingNum(const uint32_t& max_sampling_num) {
        max_sampling_num_ = max_sampling_num;
    }

    void RRTStar::setGoalSamplingRate(const double& goal_sampling_rate) {
        if(!(0.0 <= goal_sampling_rate && goal_sampling_rate <= 1.0)) {
            throw std::invalid_argument("[" + std::string(__PRETTY_FUNCTION__) + "] " +
                                        "Rate of Sampling goal state is invalid");
        }

        goal_sampling_rate_ = goal_sampling_rate;
    }

    void RRTStar::setExpandDist(const double& expand_dist) {
        expand_dist_ = expand_dist;
    }

    void RRTStar::setR(const double& R) {
        R_ = R;
    }

    bool RRTStar::solve(const State& start, const State& goal) {
        // definition of set of node
        node_list_->init();
        node_list_->add(std::make_shared<Node>(start, nullptr, 0));

        // sampling on euclidean space
        for(size_t i = 0; i < max_sampling_num_; i++) {
            auto rand_node = std::make_shared<Node>(goal, nullptr, 0);
            if(goal_sampling_rate_ < sampler_->getUniformUnitRandomVal()) {
                rand_node->state = sampler_->run(Sampler::Mode::WholeArea);

                // resample when node dose not meet constraint
                if(constraint_->checkConstraintType(rand_node->state) == ConstraintType::NOENTRY) {
                    continue;
                }
            }

            // get index of node that nearest node from sampling node
            auto nearest_node = node_list_->searchNN(rand_node);

            // generate new node
            auto new_node = generateSteerNode(nearest_node, rand_node, expand_dist_);

            // add to list if new node meets constraint
            if(constraint_->checkCollision(nearest_node->state, new_node->state)) {
                // Find nodes that exist on certain domain
                auto radius = R_ * std::pow((std::log(node_list_->getSize()) / node_list_->getSize()), 1.0 / constraint_->space.getDim());
                auto near_nodes = node_list_->searchNBHD(new_node, radius);

                // Choose parent node from near node
                updateParent(new_node, near_nodes);

                // add node to list
                node_list_->add(new_node);

                // redefine parent node of near node
                rewireNearNodes(new_node, near_nodes);

                auto cost_to_goal = new_node->state.distanceFrom(goal);
                if(cost_to_goal < expand_dist_ &&
                   new_node->cost + cost_to_goal < terminate_search_cost_) {
                    break;
                }
            }
        }

        // store the result
        result_.clear();
        auto near_goal_nodes = node_list_->searchNBHD(std::make_shared<Node>(goal, nullptr), expand_dist_);
        if(near_goal_nodes.size() == 0) {
            return false;
        }
        else {
            auto result_node = near_goal_nodes.front();
            for(const auto& near_goal_node : near_goal_nodes) {
                result_node = (near_goal_node->cost < result_node->cost) ? near_goal_node : result_node;
            }

            result_cost_ = result_node->cost + result_node->state.distanceFrom(goal);
            if(result_node->state != goal) {
                result_.push_back(goal);
            }

            while(true) {
                result_.insert(result_.begin(), result_node->state);
                if(result_node->parent == nullptr) {
                    break;
                }

                result_node = result_node->parent;
            }
            return true;
        }
    }
}
