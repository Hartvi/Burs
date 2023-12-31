#include <kdl_parser/kdl_parser.hpp>
#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/frames_io.hpp>

#include <urdf_model/model.h>
#include <urdf_parser/urdf_parser.h>

#include <string>

#include "robot_related/robot_base.h"
#include "bur_related/bur_funcs.h"

namespace Burs
{

    RobotBase::RobotBase(std::string urdf_filename)
    {
        auto opt_robot = this->GetRobotURDF(urdf_filename);

        if (!opt_robot)
        {
            throw std::runtime_error("Failed to parse robot URDF from path " + urdf_filename);
        }

        this->robot_model = opt_robot.value();

        // Create a KDL tree from the URDF model
        auto kdl_tree_res = this->GetKDLTree(robot_model);

        if (!kdl_tree_res)
        {
            throw std::runtime_error("Failed to load URDF tree.");
        }

        this->kdl_tree = kdl_tree_res.value();

        auto end_links_res = this->GetEndLinks(robot_model);

        if (!end_links_res)
        {
            throw std::runtime_error("Failed to get end links of robot (children.size() == 0).");
        }

        this->end_links = end_links_res.value();

        // std::cout << "end links:" << std::endl;
        // for (int i = 0; i < end_links.size(); ++i)
        // {
        //     std::cout << "end link: " << end_links[i] << std::endl;
        // }

        auto kdl_chain_res = this->GetKDLChain(robot_model, kdl_tree, end_links[0]);

        if (!kdl_chain_res)
        {
            throw std::runtime_error("Failed to get KDL chain.");
        }

        this->kdl_chain = kdl_chain_res.value();

        this->segmentNameToFile = this->GetSegmentNameToFile(robot_model);
        this->segmentIdToName = this->GetSegmentIdToName(this->kdl_chain);
        this->segmentIdToFile = this->GetSegmentIdToFile();

        this->minMaxBounds = this->GetMinMaxBounds();

        // this->fk_solver = std::make_shared<KDL::ChainFkSolverPos_recursive>(kdl_chain);
        // this->fk_solver = this->GetFKSolver(this->kdl_chain);

        // KDL::ChainFkSolverPos_recursive fk_solver = GetFKSolver(this->kdl_chain);

        // // Create random joint angle vector
        // std::default_random_engine generator;
        // std::uniform_real_distribution<double> distribution(-0, 0);
        // KDL::JntArray joint_positions(kdl_chain.getNrOfJoints());

        // for (unsigned int i = 0; i < joint_positions.data.size(); ++i)
        // {
        //     joint_positions(i) = distribution(generator);
        // }

        // // Compute forward kinematics
        // KDL::Frame end_effector_pose;
        // fk_solver.JntToCart(joint_positions, end_effector_pose);

        // // Print the positions of each robot segment
        // for (unsigned int i = 0; i < kdl_chain.getNrOfSegments(); ++i)
        // {
        //     KDL::Frame segment_pose;
        //     fk_solver.JntToCart(joint_positions, segment_pose, i + 1);
        //     // std::cout << "Segment " << i << ": " << segment_pose << std::endl;
        //     std::cout << "\nSegment " << i << ": \nPosition: \n"
        //               << segment_pose.p << "\nRotation: \n"
        //               << segment_pose.M << std::endl;
        // }
    }

    std::vector<std::vector<double>>
    RobotBase::GetMinMaxBounds()
    {
        std::vector<std::vector<double>> minmaxs(this->kdl_chain.getNrOfJoints(), std::vector<double>(2));

        std::vector<std::string> joint_names;
        for (int i = 0; i < kdl_chain.getNrOfSegments(); ++i)
        {
            const KDL::Segment &segment = kdl_chain.getSegment(i);
            const KDL::Joint &joint = segment.getJoint();

            if (joint.getType() != KDL::Joint::None)
            {
                joint_names.push_back(joint.getName());
            }
        }

        int num_joints = 0;
        for (const std::string &name : joint_names)
        {
            auto joint = this->robot_model->getJoint(name);
            if (joint && joint->type != urdf::Joint::UNKNOWN && joint->type != urdf::Joint::FIXED)
            {
                double lower_limit = joint->limits->lower;
                double upper_limit = joint->limits->upper;
                // Use the joint limits here
                minmaxs[num_joints][0] = lower_limit;
                minmaxs[num_joints][1] = upper_limit;
                // std::cout << "Joint " << name << ": Lower: " << lower_limit << " Upper: " << upper_limit << std::endl;
                num_joints++;
            }
        }
        if (num_joints != joint_names.size())
        {
            std::cout << "KDL chain joints: " << joint_names.size() << " URDF joints: " << num_joints << std::endl;
            throw std::runtime_error("Number of URDF joints != number of KDL chain joints.");
        }

        return minmaxs;
    }

    std::optional<urdf::ModelInterfaceSharedPtr>
    RobotBase::GetRobotURDF(std::string urdf_filename)
    {
        // Load the URDF file into a string
        std::ifstream urdf_file(urdf_filename);
        if (!urdf_file.good())
        {
            std::cerr << "Could not open file: " << urdf_filename << std::endl;
            return {};
        }

        std::string urdf_string((std::istreambuf_iterator<char>(urdf_file)),
                                std::istreambuf_iterator<char>());

        // Parse the string using the URDF parser
        urdf::ModelInterfaceSharedPtr robot_model = urdf::parseURDF(urdf_string);

        if (!robot_model)
        {
            std::cerr << "Failed to parse URDF file." << std::endl;
            return {};
        }
        return robot_model;
    }

    std::map<std::string, std::string>
    RobotBase::GetSegmentNameToFile(urdf::ModelInterfaceSharedPtr robot_model)
    {
        std::map<std::string, std::string> my_map;

        for (auto &l : robot_model->links_)
        {
            const auto &coll = l.second->collision;
            if (!coll)
            {
                continue;
            }
            const auto &geom = coll->geometry;
            if (!geom)
            {
                continue;
            }

            if (geom->type == urdf::Geometry::MESH)
            {
                const auto &mesh = std::static_pointer_cast<urdf::Mesh>(geom);
                std::string mesh_filename = mesh->filename;
                std::string link_name = l.second->name;
                // Do something with mesh_filename, which is the path to the .obj file
                // std::cout << "Link children: " << l.second->child_links.size() << " has mesh: " << mesh_filename << " mesh file name length: " << mesh_filename.size() << std::endl;
                // std::cout << "Link " << l.second->name << " has mesh: " << mesh_filename << std::endl;
                my_map[link_name] = mesh_filename;
                // std::cout << "Segment: " << my_map[link_name] << " File: " << mesh_filename << std::endl;
            }
        }

        return my_map;
    }

    std::optional<KDL::Tree>
    RobotBase::GetKDLTree(urdf::ModelInterfaceSharedPtr robot_model)
    {

        // Create a KDL tree from the URDF model
        KDL::Tree kdl_tree;
        if (!kdl_parser::treeFromUrdfModel(*robot_model, kdl_tree))
        {
            std::cerr << "Failed to construct KDL tree." << std::endl;
            return {};
        }

        return kdl_tree;
    }

    std::optional<KDL::Chain>
    RobotBase::GetKDLChain(urdf::ModelInterfaceSharedPtr robot_model, KDL::Tree kdl_tree, std::string end_effector_link)
    {
        KDL::Chain kdl_chain;
        if (!kdl_tree.getChain(robot_model->getRoot()->name, end_effector_link, kdl_chain))
        {
            std::cerr << "Failed to get KDL chain from tree." << std::endl;
            return {};
        }
        return kdl_chain;
    }

    std::optional<std::vector<std::string>>
    RobotBase::GetEndLinks(const urdf::ModelInterfaceSharedPtr &robot_model)
    {
        std::vector<std::string> end_links;
        for (auto &rl : robot_model->links_)
        {
            if (rl.second->child_links.size() == 0)
            {
                // std::cout << "Link : " << rl.second->name << " links: " << rl.second->child_links.size() << std::endl;
                end_links.push_back(rl.second->name);
            }
        }

        if (end_links.size() == 0)
        {
            return {};
        }
        else
        {
            return end_links;
        }
    }

    std::string
    RobotBase::GetLinkName(const KDL::Chain &kdl_chain, unsigned int i)
    {
        // for (unsigned int i = 0; i < kdl_chain.getNrOfSegments(); ++i)
        // {
        //     std::string segment_name = kdl_chain.getSegment(i).getName();
        //     std::cout << "Segment " << i << ": " << segment_name << " File: " << my_map[segment_name] << std::endl;
        // }
        return kdl_chain.getSegment(i).getName();
    }

    std::map<int, std::string>
    RobotBase::GetSegmentIdToName(const KDL::Chain &kdl_chain)
    {
        std::map<int, std::string> id_to_model_name;
        for (unsigned int i = 0; i < kdl_chain.getNrOfSegments(); ++i)
        {
            std::string segment_name = kdl_chain.getSegment(i).getName();
            // std::cout << "Segment name: " << segment_name << std::endl;
            // std::cout << "Segment name length: " << segment_name.size() << std::endl;
            id_to_model_name[i] = segment_name;
            // std::cout << "Id: " << i << " Segment: " << segment_name << std::endl;
        }
        return id_to_model_name;
    }

    std::map<int, std::string>
    RobotBase::GetSegmentIdToFile()
    {
        std::map<int, std::string> segmentIdToFile;

        for (const auto &entry : this->segmentIdToName)
        {
            int key = entry.first;
            // Check if 'segmentToFile' contains the 'key'
            if (this->segmentNameToFile.find(entry.second) != this->segmentNameToFile.end())
            {
                segmentIdToFile[key] = this->segmentNameToFile[entry.second];
                // std::cout << "Segment " << key << ": "
                //           << " File: " << this->segmentNameToFile[entry.second] << std::endl;
            }
        }

        return segmentIdToFile;
    }

    KDL::ChainFkSolverPos_recursive
    RobotBase::GetFKSolver(KDL::Chain kdl_chain)
    {
        KDL::ChainFkSolverPos_recursive fk_solver = KDL::ChainFkSolverPos_recursive(kdl_chain);
        return fk_solver;
    }

    std::tuple<std::vector<Eigen::Matrix3d>, std::vector<Eigen::Vector3d>>
    RobotBase::ForwardQ(const Eigen::VectorXd &q_in)
    {
        KDL::ChainFkSolverPos_recursive fk_solver(this->kdl_chain);

        // Convert Eigen vector to KDL joint array
        unsigned int num_joints = this->kdl_chain.getNrOfJoints();

        assert(num_joints == q_in.size());

        KDL::JntArray joint_positions(num_joints);
        for (unsigned int i = 0; i < joint_positions.data.size(); ++i)
        {
            joint_positions(i) = q_in(i);
        }

        // Prepare the output vectors for rotations and positions
        std::vector<Eigen::Matrix3d> rotations;
        std::vector<Eigen::Vector3d> positions;

        // Compute forward kinematics for all segments
        for (unsigned int i = 0; i < this->kdl_chain.getNrOfSegments(); ++i)
        {
            KDL::Frame segment_pose;
            if (fk_solver.JntToCart(joint_positions, segment_pose, i + 1) >= 0)
            {
                // Convert KDL rotation to Eigen matrix
                Eigen::Matrix3d rotation;
                for (int j = 0; j < 3; ++j)
                {
                    for (int k = 0; k < 3; ++k)
                    {
                        rotation(j, k) = segment_pose.M(j, k);
                    }
                }
                rotations.push_back(rotation);

                // Convert KDL position to Eigen vector
                Eigen::Vector3d position(segment_pose.p.x(), segment_pose.p.y(), segment_pose.p.z());
                // std::cout << "Segment " << i << ": " << position.transpose() << std::endl;
                positions.push_back(position);
            }
            else
            {
                // Handle the error case if FK solver failed
                // Depending on how you want to handle errors, you might throw an exception
                // or handle it in some other way
                // std::cout << "segment pose: " << segment_pose << std::endl;
                throw std::runtime_error("Forward kinematics solver failed at segment " + std::to_string(i));
            }
        }

        // Return the tuple of rotations and positions
        return std::make_tuple(rotations, positions);
    }

    // using ForwardKinematics = std::function<Vector3d(const int &ith_distal_point, const VectorXd &configuration)>;
    Eigen::Vector3d
    RobotBase::GetForwardPoint(const int &ith_distal_point, const Eigen::VectorXd &q_in)
    {
        KDL::ChainFkSolverPos_recursive fk_solver(this->kdl_chain);
        // Convert Eigen vector to KDL joint array
        size_t num_joints = this->kdl_chain.getNrOfJoints();

        assert(q_in.size() == num_joints);

        KDL::JntArray joint_positions(num_joints);

        // theoretically don't need the joint angles after (i+1)
        for (unsigned int i = 0; i < num_joints; ++i)
        {
            joint_positions(i) = q_in(i);
        }

        KDL::Frame segment_pose;
        if (fk_solver.JntToCart(joint_positions, segment_pose, ith_distal_point) >= 0)
        {

            // Convert KDL position to Eigen vector
            Eigen::Vector3d position(segment_pose.p.x(), segment_pose.p.y(), segment_pose.p.z());
            // std::cout << "Forward point " << ith_distal_point << ": " << position.transpose() << std::endl;
            return position;
        }
        else
        {
            // Handle the error case if FK solver failed
            // Depending on how you want to handle errors, you might throw an exception
            // or handle it in some other way
            // std::cout << "segment pose: " << segment_pose << std::endl;
            throw std::runtime_error("Forward kinematics solver failed at segment " + std::to_string(ith_distal_point));
        }
    }

    ForwardKinematics RobotBase::GetForwardPointFunc()
    {
        ForwardKinematics fpf = [this](const int &ith_distal_point, const VectorXd &configuration) -> Vector3d
        {
            return this->GetForwardPoint(ith_distal_point, configuration);
        };
        return fpf;
    }

    double
    RobotBase::GetRadius(const int &ith_distal_point, const Eigen::VectorXd &q_in)
    {
        KDL::ChainFkSolverPos_recursive fk_solver(this->kdl_chain);

        size_t num_joints = this->kdl_chain.getNrOfJoints();

        assert(q_in.size() == num_joints);

        KDL::JntArray joint_positions(num_joints);

        for (unsigned int i = 0; i < num_joints; ++i)
        {
            joint_positions(i) = q_in(i);
        }

        KDL::Frame segment_pose;
        if (fk_solver.JntToCart(joint_positions, segment_pose, ith_distal_point) >= 0)
        {
            Eigen::Vector3d position(segment_pose.p.x(), segment_pose.p.y(), segment_pose.p.z());
            // std::cout << "GetRadius position: " << position.transpose() << std::endl;

            KDL::Vector joint_axis_local = this->kdl_chain.getSegment(ith_distal_point).getJoint().JointAxis();

            KDL::Frame end_effector_pose;
            if (fk_solver.JntToCart(joint_positions, end_effector_pose) >= 0)
            {
                KDL::Vector end_effector_kdl = end_effector_pose.p;
                Eigen::Vector3d end_effector(end_effector_kdl.x(), end_effector_kdl.y(), end_effector_kdl.z());

                // Transform the local joint axis to the world reference frame
                KDL::Vector joint_axis_world = segment_pose.M * joint_axis_local;
                // std::cout << "GetRadius axis: " << joint_axis_world << std::endl;
                Eigen::Vector3d joint_axis(joint_axis_world.x(), joint_axis_world.y(), joint_axis_world.z());

                Eigen::Vector3d diff = end_effector - position;
                // Project the end effector onto the plane defined by the joint axis
                double dot_product = diff.dot(joint_axis);
                // joint_axis has norm = 1 => NO NORMALIZATION NECESSARY
                Eigen::Vector3d projection = diff - dot_product * joint_axis;

                // Return the norm (distance) of the projection

                // std::cout << "Radius distance " << ith_distal_point << ": " << projection.norm() << std::endl;
                return projection.norm();
            }
            else
            {
                throw std::runtime_error("Failed to calculate end-effector pose for radius calculation.");
            }
        }
        else
        {
            throw std::runtime_error("Forward kinematics solver failed at segment " + std::to_string(ith_distal_point));
        }
    }

    RadiusFunc
    RobotBase::GetRadiusFunc()
    {
        RadiusFunc rf = [this](const int &ith_distal_point, const VectorXd &configuration) -> double
        {
            return this->GetRadius(ith_distal_point, configuration);
        };
        return rf;
    }
}