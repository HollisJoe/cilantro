#pragma once

#include <cilantro/convex_polytope.hpp>

namespace cilantro {
    template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
    class SpaceRegion {
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        SpaceRegion() {}

        SpaceRegion(const std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > &polytopes) : polytopes_(polytopes) {}

        SpaceRegion(const ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> &polytope) : polytopes_(std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> >(1,polytope)) {}

        SpaceRegion(const ConstDataMatrixMap<InputScalarT,EigenDim> &points, bool compute_topology = false, bool simplicial_facets = false, double merge_tol = 0.0) {
            polytopes_.emplace_back(points, compute_topology, simplicial_facets, merge_tol);
        }

        SpaceRegion(const ConstDataMatrixMap<InputScalarT,EigenDim+1> &halfspaces, bool compute_topology = false, bool simplicial_facets = false, double dist_tol = std::numeric_limits<InputScalarT>::epsilon(), double merge_tol = 0.0) {
            polytopes_.emplace_back(halfspaces, compute_topology, simplicial_facets, dist_tol, merge_tol);
        }

        ~SpaceRegion() {}

        SpaceRegion unionWith(const SpaceRegion &sr) const {
            std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > res_polytopes(polytopes_);
            res_polytopes.insert(res_polytopes.end(), sr.polytopes_.begin(), sr.polytopes_.end());
            return SpaceRegion(std::move(res_polytopes));
        }

        SpaceRegion intersectionWith(const SpaceRegion &sr, bool compute_topology = false, bool simplicial_facets = false, double dist_tol = std::numeric_limits<InputScalarT>::epsilon(), double merge_tol = 0.0) const {
            std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > res_polytopes;
            for (size_t i = 0; i < polytopes_.size(); i++) {
                for (size_t j = 0; j < sr.polytopes_.size(); j++) {
                    ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> poly_tmp(polytopes_[i].intersectionWith(sr.polytopes_[j], compute_topology, simplicial_facets, dist_tol, merge_tol));
                    if (!poly_tmp.isEmpty()) {
                        res_polytopes.emplace_back(std::move(poly_tmp));
                    }
                }
            }
            return SpaceRegion(std::move(res_polytopes));
        }

        // Inefficient
        SpaceRegion complement(bool compute_topology = false, bool simplicial_facets = false, double dist_tol = std::numeric_limits<InputScalarT>::epsilon(), double merge_tol = 0.0) const {
            std::vector<std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > > tuples;
            tuples.emplace_back(0);
            for (size_t p = 0; p < polytopes_.size(); p++) {
                const std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> >& hs(polytopes_[p].getFacetHyperplanes());
                std::vector<std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > > tuples_new;
                for (size_t t = 0; t < tuples.size(); t++) {
                    std::vector<Eigen::Matrix<OutputScalarT,EigenDim+1,1> > tup_curr(tuples[t]);
                    tup_curr.resize(tup_curr.size() + 1);
                    for (size_t h = 0; h < hs.size(); h++) {
                        tup_curr[tup_curr.size()-1] = -hs[h];
                        tuples_new.emplace_back(tup_curr);
                    }
                }
                tuples = std::move(tuples_new);
            }

//        std::cout << "Generated " << tuples.size() << " tuples!" << std::endl;

            std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > res_polytopes;
            for (size_t t = 0; t < tuples.size(); t++) {

//            std::cout << "Tuple " << t << ":" << std::endl;
//            for (size_t tt = 0; tt < tuples[t].size(); tt++) {
//                std::cout << tuples[t][tt].transpose() << ",   ";
//            }
//            std::cout << std::endl;

                ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> poly_tmp(tuples[t], compute_topology, simplicial_facets, dist_tol, merge_tol);
                if (!poly_tmp.isEmpty()) {
                    res_polytopes.emplace_back(std::move(poly_tmp));
                }
            }
            return SpaceRegion(std::move(res_polytopes));
        }

        SpaceRegion relativeComplement(const SpaceRegion &sr, bool compute_topology = false, bool simplicial_facets = false, double dist_tol = std::numeric_limits<InputScalarT>::epsilon(), double merge_tol = 0.0) const {
            return intersectionWith(sr.complement(compute_topology, simplicial_facets, dist_tol, merge_tol), compute_topology, simplicial_facets, dist_tol, merge_tol);
        }

        inline bool isEmpty() const {
            for (size_t i = 0; i < polytopes_.size(); i++) {
                if (!polytopes_[i].isEmpty()) return false;
            }
            return true;
        }

        inline bool isBounded() const {
            for (size_t i = 0; i < polytopes_.size(); i++) {
                if (!polytopes_[i].isBounded()) return false;
            }
            return true;
        }

        // Inefficient
        double getVolume(double dist_tol = std::numeric_limits<InputScalarT>::epsilon(), double merge_tol = 0.0) const {
            if (!isBounded()) return std::numeric_limits<double>::infinity();

            std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > subsets;
            subsets.emplace_back(std::vector<Eigen::Matrix<InputScalarT,EigenDim+1,1> >(0));
            std::vector<size_t> subset_sizes;
            subset_sizes.emplace_back(0);
            double volume = 0.0;
            for (size_t i = 0; i < polytopes_.size(); i++) {
                std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > subsets_tmp(subsets);
                std::vector<size_t> subset_sizes_tmp(subset_sizes);
                for (size_t j = 0; j < subsets_tmp.size(); j++) {
                    subsets_tmp[j] = subsets_tmp[j].intersectionWith(polytopes_[i], false, false, dist_tol, merge_tol);
                    subset_sizes_tmp[j]++;
                    volume += (2.0*(subset_sizes_tmp[j]%2) - 1.0)*subsets_tmp[j].getVolume();
                }
                std::move(std::begin(subsets_tmp), std::end(subsets_tmp), std::back_inserter(subsets));
                std::move(std::begin(subset_sizes_tmp), std::end(subset_sizes_tmp), std::back_inserter(subset_sizes));
            }

            return volume;
        }

        inline const std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> >& getConvexPolytopes() const { return polytopes_; }

        inline const Eigen::Matrix<OutputScalarT,EigenDim,1>& getInteriorPoint() const {
            for (size_t i = 0; i < polytopes_.size(); i++) {
                if (!polytopes_[i].isEmpty()) return polytopes_[i].getInteriorPoint();
            }
            return nan_point_;
        }

        inline bool containsPoint(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,1> > &point, OutputScalarT offset = 0.0) const {
            for (size_t i = 0; i < polytopes_.size(); i++) {
                if (polytopes_[i].containsPoint(point, offset)) return true;
            }
            return false;
        }

        Eigen::Matrix<bool,1,Eigen::Dynamic> getInteriorPointsIndexMask(const ConstDataMatrixMap<OutputScalarT,EigenDim> &points, OutputScalarT offset = 0.0) const {
            Eigen::Matrix<bool,1,Eigen::Dynamic> mask(1,points.cols());
            for (size_t i = 0; i < points.cols(); i++) {
                mask(i) = containsPoint(points.col(i), offset);
            }
            return mask;
        }

        std::vector<size_t> getInteriorPointIndices(const ConstDataMatrixMap<OutputScalarT,EigenDim> &points, OutputScalarT offset = 0.0) const {
            std::vector<size_t> indices;
            indices.reserve(points.cols());
            for (size_t i = 0; i < points.cols(); i++) {
                if (containsPoint(points.col(i), offset)) indices.emplace_back(i);
            }
            return indices;
        }

        SpaceRegion& transform(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,EigenDim> > &rotation, const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim,1> > &translation) {
            for (size_t i = 0; i < polytopes_.size(); i++) {
                polytopes_[i].transform(rotation, translation);
            }
            return *this;
        }

        SpaceRegion& transform(const Eigen::Ref<const Eigen::Matrix<OutputScalarT,EigenDim+1,EigenDim+1> > &rigid_transform) {
            return transform(rigid_transform.topLeftCorner(EigenDim,EigenDim), rigid_transform.topRightCorner(EigenDim,1));
        }

    protected:
        std::vector<ConvexPolytope<InputScalarT,OutputScalarT,EigenDim> > polytopes_;

        static Eigen::Matrix<OutputScalarT,EigenDim,1> nan_point_;

    };

    template <typename InputScalarT, typename OutputScalarT, ptrdiff_t EigenDim>
    Eigen::Matrix<OutputScalarT,EigenDim,1> SpaceRegion<InputScalarT,OutputScalarT,EigenDim>::nan_point_ = Eigen::Matrix<OutputScalarT,EigenDim,1>::Constant(std::numeric_limits<OutputScalarT>::quiet_NaN());

    typedef SpaceRegion<float,float,2> SpaceRegion2D;
    typedef SpaceRegion<float,float,3> SpaceRegion3D;
}
