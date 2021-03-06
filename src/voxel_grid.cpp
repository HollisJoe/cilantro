#include <cilantro/voxel_grid.hpp>

namespace cilantro {
    VoxelGrid::VoxelGrid(const std::vector<Eigen::Vector3f> &points, float bin_size)
            : input_points_(&points),
              input_normals_(NULL),
              input_colors_(NULL),
              bin_size_(bin_size),
              empty_indices_(0)
    {
        build_lookup_table_();
    }

    VoxelGrid::VoxelGrid(const PointCloud &cloud, float bin_size)
            : input_points_(&cloud.points),
              input_normals_(cloud.hasNormals()?&cloud.normals:NULL),
              input_colors_(cloud.hasColors()?&cloud.colors:NULL),
              bin_size_(bin_size),
              empty_indices_(0)
    {
        build_lookup_table_();
    }

    std::vector<Eigen::Vector3f> VoxelGrid::getDownsampledPoints(size_t min_points_in_bin) const {
        std::vector<Eigen::Vector3f> points;
        points.reserve(grid_lookup_table_.size());

        Eigen::Vector3f point;
        float scale;

        for (size_t k = 0; k < map_iterators_.size(); k++) {
            auto it = map_iterators_[k];

            const std::vector<size_t>& bin_ind(it->second);
            if (bin_ind.size() < min_points_in_bin) continue;

            scale = 1.0f/bin_ind.size();

            point.setZero();
            for (size_t i = 0; i < bin_ind.size(); i++) {
                point += (*input_points_)[bin_ind[i]];
            }

            points.emplace_back(scale*point);
        }

        return points;
    }

    std::vector<Eigen::Vector3f> VoxelGrid::getDownsampledNormals(size_t min_points_in_bin) const {
        std::vector<Eigen::Vector3f> normals;
        if (input_normals_ == NULL) {
            return normals;
        }

//    normals.reserve(grid_lookup_table_.size());
//    for (auto it = grid_lookup_table_.begin(); it != grid_lookup_table_.end(); ++it) {
//        if (it->second.size() < min_points_in_bin) continue;
//
//        Eigen::MatrixXf bin_normals(3, it->second.size()*2);
//        Eigen::Vector3f ref_dir = (*input_normals_)[it->second[0]];
//        size_t pos = 0, neg = 0;
//        for (size_t i = 0; i < it->second.size(); i++) {
//            const Eigen::Vector3f& curr_normal = (*input_normals_)[it->second[i]];
//            if (ref_dir.dot(curr_normal) < 0.0f) neg++; else pos++;
//            bin_normals.col(2*i) = -curr_normal;
//            bin_normals.col(2*i+1) = curr_normal;
//        }
//        if (neg > pos) ref_dir = -ref_dir;
//
//        PCA3D pca(bin_normals.data(), 3, it->second.size()*2);
//        Eigen::Vector3f avg = pca.getEigenVectorsMatrix().col(0);
//        if (ref_dir.dot(avg) < 0.0f) {
//            normals.emplace_back(-avg);
//        } else {
//            normals.emplace_back(avg);
//        }
//    }

        normals.reserve(grid_lookup_table_.size());

        Eigen::Vector3f normal;

        for (size_t k = 0; k < map_iterators_.size(); k++) {
            auto it = map_iterators_[k];

            const std::vector<size_t>& bin_ind(it->second);
            if (bin_ind.size() < min_points_in_bin) continue;

            normal.setZero();
            Eigen::Vector3f ref_dir = (*input_normals_)[bin_ind[0]];
            size_t pos = 0, neg = 0;
            for (size_t i = 0; i < bin_ind.size(); i++) {
                const Eigen::Vector3f& curr_normal = (*input_normals_)[bin_ind[i]];
                if (ref_dir.dot(curr_normal) < 0.0f) {
                    normal -= curr_normal;
                    neg++;
                } else {
                    normal += curr_normal;
                    pos++;
                }
            }
            if (neg > pos) normal *= -1.0f;

            normals.emplace_back(normal.normalized());
        }

        return normals;
    }

    std::vector<Eigen::Vector3f> VoxelGrid::getDownsampledColors(size_t min_points_in_bin) const {
        std::vector<Eigen::Vector3f> colors;
        if (input_colors_ == NULL) return colors;

        colors.reserve(grid_lookup_table_.size());

        Eigen::Vector3f color;
        float scale;

        for (size_t k = 0; k < map_iterators_.size(); k++) {
            auto it = map_iterators_[k];

            const std::vector<size_t>& bin_ind(it->second);
            if (bin_ind.size() < min_points_in_bin) continue;

            scale = 1.0f/bin_ind.size();

            color.setZero();
            for (size_t i = 0; i < bin_ind.size(); i++) {
                color += (*input_colors_)[bin_ind[i]];
            }

            colors.emplace_back(scale*color);
        }

        return colors;
    }

    PointCloud VoxelGrid::getDownsampledCloud(size_t min_points_in_bin) const {
        std::vector<Eigen::Vector3f> points, normals, colors;
        Eigen::Vector3f point, normal, color;

        bool do_normals = input_normals_ != NULL;
        bool do_colors = input_colors_ != NULL;

        points.reserve(grid_lookup_table_.size());
        if (do_normals) normals.reserve(grid_lookup_table_.size());
        if (do_colors) colors.reserve(grid_lookup_table_.size());

        float scale;

//    for (auto it = grid_lookup_table_.begin(); it != grid_lookup_table_.end(); ++it) {
//#pragma omp parallel for shared (points, normals, colors) private (point, normal, color, scale) num_threads(4)
        for (size_t k = 0; k < map_iterators_.size(); k++) {
            auto it = map_iterators_[k];

            const std::vector<size_t>& bin_ind(it->second);
            if (bin_ind.size() < min_points_in_bin) continue;

            scale = 1.0f/bin_ind.size();

            point.setZero();
            for (size_t i = 0; i < bin_ind.size(); i++) {
                point += (*input_points_)[bin_ind[i]];
            }

            points.emplace_back(scale*point);

            if (do_normals) {
                normal.setZero();
                Eigen::Vector3f ref_dir = (*input_normals_)[bin_ind[0]];
                size_t pos = 0, neg = 0;
                for (size_t i = 0; i < bin_ind.size(); i++) {
                    const Eigen::Vector3f& curr_normal = (*input_normals_)[bin_ind[i]];
                    if (ref_dir.dot(curr_normal) < 0.0f) {
                        normal -= curr_normal;
                        neg++;
                    } else {
                        normal += curr_normal;
                        pos++;
                    }
                }
                if (neg > pos) normal *= -1.0f;

                normals.emplace_back(normal.normalized());
            }

            if (do_colors) {
                color.setZero();
                for (size_t i = 0; i < bin_ind.size(); i++) {
                    color += (*input_colors_)[bin_ind[i]];
                }

                colors.emplace_back(scale*color);
            }

//#pragma omp critical
//        {
//            points.emplace_back(scale*point);
//            if (do_normals) normals.emplace_back(normal.normalized());
//            if (do_colors) colors.emplace_back(scale*color);
//        }

        }

        return PointCloud(points, normals, colors);
    }

    const std::vector<size_t>& VoxelGrid::getGridBinNeighbors(const Eigen::Vector3f &point) const {
        std::array<int,3> grid_coords = {(int)((point[0] - min_pt_[0])/bin_size_), (int)((point[1] - min_pt_[1])/bin_size_), (int)((point[2] - min_pt_[2])/bin_size_)};
        auto it = grid_lookup_table_.find(grid_coords);
        if (it == grid_lookup_table_.end()) return empty_indices_;
        return it->second;
    }

    const std::vector<size_t>& VoxelGrid::getGridBinNeighbors(size_t point_ind) const {
        return VoxelGrid::getGridBinNeighbors((*input_points_)[point_ind]);
    }

    void VoxelGrid::build_lookup_table_() {
        if (input_points_->empty()) return;

        min_pt_[0] = std::numeric_limits<float>::infinity();
        min_pt_[1] = std::numeric_limits<float>::infinity();
        min_pt_[2] = std::numeric_limits<float>::infinity();
        for (size_t i = 0; i < input_points_->size(); i++) {
            if ((*input_points_)[i][0] < min_pt_[0]) min_pt_[0] = (*input_points_)[i][0];
            if ((*input_points_)[i][1] < min_pt_[1]) min_pt_[1] = (*input_points_)[i][1];
            if ((*input_points_)[i][2] < min_pt_[2]) min_pt_[2] = (*input_points_)[i][2];
        }
        // Round to integer grid coordinates
        min_pt_[0] = std::floor(min_pt_[0]/bin_size_)*bin_size_;
        min_pt_[1] = std::floor(min_pt_[1]/bin_size_)*bin_size_;
        min_pt_[2] = std::floor(min_pt_[2]/bin_size_)*bin_size_;

        map_iterators_.reserve(input_points_->size());
        std::array<int,3> grid_coords;
        for (size_t i = 0; i < input_points_->size(); i++) {
            grid_coords[0] = (int)(((*input_points_)[i][0] - min_pt_[0])/bin_size_);
            grid_coords[1] = (int)(((*input_points_)[i][1] - min_pt_[1])/bin_size_);
            grid_coords[2] = (int)(((*input_points_)[i][2] - min_pt_[2])/bin_size_);

            auto lb = grid_lookup_table_.lower_bound(grid_coords);
            if(lb != grid_lookup_table_.end() && !(grid_lookup_table_.key_comp()(grid_coords, lb->first))) {
                lb->second.emplace_back(i);
            } else {
                map_iterators_.emplace_back(grid_lookup_table_.emplace_hint(lb, grid_coords, std::vector<size_t>(1, i)));
            }
        }
    }
}
