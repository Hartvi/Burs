#include <iostream>
#include <string>
#include <Eigen/Dense>
#include "model_related/pqp_load.h"
#include "model_related/rt_model.h"
#include <flann/flann.hpp>
#include "printing.h"
#include "bur_related/bur_tree.h"
#include "bur_related/base_planner.h"
#include <functional>
#include "math.h"
#include "bur_related/bur_funcs.h"
#include "test_related/stick_robot.h"
#include <fstream>
// #include <yaml-cpp/yaml.h>
#include <cstdlib>

#include "test_related/test_urdf.h"

namespace test
{
    using namespace std;
    using namespace Burs;

    Eigen::Matrix3d eulerToRotationMatrix(double x, double y, double z)
    {
        Eigen::Matrix3d Rx, Ry, Rz;

        // Rotation matrix around the X-axis
        Rx << 1, 0, 0,
            0, cos(x), -sin(x),
            0, sin(x), cos(x);

        // Rotation matrix around the Y-axis
        Ry << cos(y), 0, sin(y),
            0, 1, 0,
            -sin(y), 0, cos(y);

        // Rotation matrix around the Z-axis
        Rz << cos(z), -sin(z), 0,
            sin(z), cos(z), 0,
            0, 0, 1;

        // Combined rotation matrix, R = Rz * Ry * Rx
        Eigen::Matrix3d R = Rz * Ry * Rx;

        return R;
    }

    void main_test()
    {
        // TODO
        // test
        // visualize path
        std::string obstacle_path = "/home/hartvi/Documents/CVUT/diploma_thesis/Models/cube.obj";
        Eigen::Vector3d obstacle_position = Eigen::Vector3d(-0.5, -1.5, -0.5);
        Eigen::Vector3d obstacle_position2 = Eigen::Vector3d(1.6477, 0.25852, 0.66459);
        std::string robot_urdf_path = "/home/hartvi/Documents/CVUT/diploma_thesis/burs_of_free_space/jogramop/robots/franka_panda/mobile_panda.urdf";

        // URDFPlanner(std::string urdf_file, int max_iters, double d_crit, double delta_q, double epsilon_q, int num_spikes);
        int max_iters = 5;
        Meters d_crit = 0.1;
        Relative delta_q = 1.0;
        Relative epsilon_q = 0.028;
        int num_spikes = 7;
        URDFPlanner urdf_planner(robot_urdf_path, max_iters, d_crit, delta_q, epsilon_q, num_spikes);

        urdf_planner.AddObstacle(obstacle_path, eulerToRotationMatrix(1.57, 0, 0), obstacle_position);
        urdf_planner.AddObstacle(obstacle_path, eulerToRotationMatrix(1.57, 0, 0), obstacle_position2);

        Eigen::VectorXd zero_config = Eigen::VectorXd::Zero(urdf_planner.GetNrOfJoints());
        Eigen::VectorXd min_q = Eigen::VectorXd(urdf_planner.GetNrOfJoints());
        Eigen::VectorXd max_q = Eigen::VectorXd(urdf_planner.GetNrOfJoints());
        auto minmax = urdf_planner.mCollisionEnv->myURDFRobot->GetMinMaxBounds();

        for (int i = 0; i < min_q.size(); ++i)
        {
            min_q(i) = minmax[i][0];
            max_q(i) = minmax[i][1];
        }
        std::cout << "Min q: " << min_q.transpose() << std::endl;
        std::cout << "max q: " << max_q.transpose() << std::endl;
        min_q(0) = 0;
        min_q(1) = 0;
        max_q(0) = 0;
        max_q(1) = 0;

        // plan path
        Eigen::VectorXd start = min_q;
        Eigen::VectorXd goal = max_q;

        auto opt_path = urdf_planner.PlanPath(start, goal);

        std::vector<Eigen::VectorXd> path;
        if (opt_path)
        {
            path = opt_path.value();
            std::cout << "START: Interpolate path" << std::endl;
            path = URDFPlanner::InterpolatePath(path);
            std::cout << "FINISH: Interpolate path. Length: " << path.size() << std::endl;
        }
        else
        {
            std::cout << "URDFPlanner: PlanPath: failed to plan path from " << start.transpose() << " to " << goal.transpose() << std::endl;
            std::cout << "Quitting..." << std::endl;
            return;
        }

        std::string test_file_name = "test_file.txt";
        std::ofstream test_file(test_file_name);
        // for (int i = 0; i <)
        //     urdf_planner.mCollisionEnv->SetPoses(q_waypoint);
        std::cout << "Closest distance from robot to obstacles: " << urdf_planner.mCollisionEnv->GetClosestDistance() << std::endl;
        // urdf_planner.mCollisionEnv->myURDFRobot->
        if (test_file.is_open())
        {
            for (Eigen::VectorXd &point : path)
            {
                test_file << urdf_planner.ToString(point);
            }
            test_file.close();
            const char *command = "python3.10 ../scripts/render_scene.py test_file.txt";
            int result = system(command);

            if (result != 0)
            {
                std::cout << "Calling `" << command << "` failed" << std::endl;
            }
        }
        else
        {
            std::cout << "Could not open " << test_file_name << std::endl;
        }

        // auto opt_path = urdf_planner.PlanPath();
    }

    // void testFunctions(int argc, char *argv[])
    // {
    //     if (argc < 3)
    //     {
    //         std::cout << "Usage: ./exc robot_model.obj obstacle_model.obj" << std::endl;
    //         return;
    //     }

    //     std::shared_ptr<StickRobot> stick_robot = std::make_shared<StickRobot>();

    //     Eigen::MatrixXd bounds = Eigen::MatrixXd::Zero(stick_robot->NUM_SEGMENTS, 2);
    //     for (int i = 0; i < stick_robot->NUM_SEGMENTS; ++i)
    //     {
    //         bounds.col(0).setConstant(-M_PI);
    //         bounds.col(1).setConstant(M_PI);
    //     }
    //     std::cout << "bounds:\n"
    //               << bounds << std::endl;

    //     // Number of tests
    //     const int numTests = 1;

    //     // Create random generator and uniform distributions for the angles
    //     Eigen::VectorXd randConfiguration(2);
    //     std::default_random_engine generator;
    //     std::uniform_real_distribution<double> distribution(-M_PI, M_PI); // Assume angles are in range [-pi, pi]

    //     for (int i = 0; i < numTests; i++)
    //     {
    //         // Generate random configuration
    //         randConfiguration << M_PI_2, 0.0;

    //         std::cout << "Test " << i + 1 << ":" << std::endl;
    //         std::cout << "Random Configuration: [" << randConfiguration(0) << ", " << randConfiguration(1) << "]" << std::endl;

    //         for (int ith_distal_point = 0; ith_distal_point <= 1; ith_distal_point++)
    //         {
    //             std::cout << "For ith_distal_point: " << ith_distal_point << std::endl;

    //             // ForwardJoint
    //             Eigen::Vector3d resultFK = stick_robot->ForwardJoint(ith_distal_point, randConfiguration);
    //             std::cout << "ForwardJoint Result: [" << resultFK(0) << ", " << resultFK(1) << ", " << resultFK(2) << "]" << std::endl;

    //             // RadiusFunc
    //             double resultRF = stick_robot->RadiusFunc(ith_distal_point, randConfiguration);
    //             std::cout << "RadiusFunc Result: " << resultRF << std::endl;

    //             // StickRt
    //             auto resultSRT = stick_robot->StickRt(randConfiguration);
    //             std::vector<Eigen::Matrix3d> rotations = std::get<0>(resultSRT);
    //             std::vector<Eigen::Vector3d> translations = std::get<1>(resultSRT);
    //             for (size_t j = 0; j < rotations.size(); j++)
    //             {
    //                 std::cout << "Rotation Matrix " << j + 1 << ":" << std::endl
    //                           << rotations[j] << std::endl;
    //                 std::cout << "Translation Vector " << j + 1 << ": [" << translations[j](0) << ", " << translations[j](1) << ", " << translations[j](2) << "]" << std::endl;
    //             }

    //             std::cout << "-----------------------" << std::endl;
    //         }
    //     }
    // }

    // void test_forward(int argc, char *argv[])
    // {
    //     // std::cout << "TEST FUNCTIONS" << std::endl;
    //     // testFunctions(argc, argv);
    //     // std::cout << "END TEST FUNCTIONS" << std::endl;
    //     std::cout << std::endl;

    //     if (argc < 6)
    //     {
    //         std::cout << "Usage: ./exc robot_model.obj obstacle_model.obj x y z" << std::endl;
    //         return;
    //     }
    //     std::cout << "FUNCTION TESTING" << std::endl;

    //     std::shared_ptr<StickRobot> stick_robot = std::make_shared<StickRobot>();

    //     Eigen::MatrixXd bounds = Eigen::MatrixXd::Zero(stick_robot->NUM_SEGMENTS, 2);
    //     for (int i = 0; i < stick_robot->NUM_SEGMENTS; ++i)
    //     {
    //         bounds.col(0).setConstant(-10 * M_PI);
    //         bounds.col(1).setConstant(10 * M_PI);
    //     }
    //     std::cout << "bounds:\n"
    //               << bounds << std::endl;

    //     // Burs::ForwardKinematics fk = ForwardDistal;
    //     // BurNodeWrapper(ForwardKinematics f, int num_segments, int max_iters, double d_crit, double delta_q, double epsilon_q);
    //     // Burs::ForwardKinematics fk = stick_robot->ForwardJoint;

    //     Burs::ForwardKinematics fk =
    //         [stick_robot](const int &ith_distal_point, const Eigen::VectorXd &configuration) -> Eigen::Vector3d
    //     {
    //         return stick_robot->ForwardJoint(ith_distal_point, configuration);
    //     };

    //     Burs::RadiusFunc rf = [stick_robot](const int &ith_distal_point, const Eigen::VectorXd &configuration) -> double
    //     {
    //         return stick_robot->RadiusFunc(ith_distal_point, configuration);
    //     };

    //     Burs::ForwardRt srt = [stick_robot](const Eigen::VectorXd &configuration) -> std::tuple<std::vector<Matrix3d>, std::vector<Vector3d>>
    //     {
    //         return stick_robot->StickRt(configuration);
    //     };

    //     int max_iters = 100;
    //     double d_crit = 0.1;
    //     double delta_q = M_PI;
    //     double epsilon_q = M_PI / 10.0;
    //     int num_spikes = 7;
    //     std::unique_ptr<Burs::BasePlanner> b = std::make_unique<Burs::BasePlanner>(stick_robot->NUM_SEGMENTS, fk, max_iters, d_crit, delta_q, epsilon_q, bounds, rf, num_spikes);

    //     int point = 1;
    //     VectorXd config(2);
    //     VectorXd config2(2);
    //     config << 0, 0;
    //     config2 << 4.5 * M_PI, 0;

    //     // std::cout << "config1: " << config.transpose() << std::endl;
    //     // std::cout << "config2: " << config2.transpose() << std::endl;

    //     Eigen::Vector3d result = b->ForwardEuclideanJoint(1, config); // Call the function through the std::function wrapper

    //     // Output
    //     std::cout << "Initial config distal point: " << result.transpose() << std::endl;

    //     std::cout << "END FUNCTION TESTING" << std::endl;
    //     std::cout << std::endl;
    //     // TESTING TOGETHER

    //     std::shared_ptr<Burs::BaseEnv> b_env = std::make_shared<Burs::BaseEnv>();

    //     std::shared_ptr<RtModels::RtModel> robot_model = std::make_shared<RtModels::RtModel>(argv[1]);
    //     std::shared_ptr<RtModels::RtModel> obstacle_model = std::make_shared<RtModels::RtModel>(argv[2]);

    //     b_env->AddRobotModel(robot_model);
    //     b_env->AddForwardRt(srt);
    //     b_env->AddObstacle(argv[2]);

    //     MatrixXd m(6, 3);
    //     std::cout << "m: " << m << std::endl;
    //     Vector3d obstacle_position = Vector3d(std::atof(argv[3]), std::atof(argv[4]), std::atof(argv[5]));
    //     for (int i = 0; i < b_env->obstacle_models.size(); i++)
    //     {
    //         // for cube
    //         std::cout << "Obstacle position: " << Eigen::Vector3d(b_env->obstacle_models[i]->getT()).transpose() << std::endl;
    //         b_env->obstacle_models[i]->SetTranslation(obstacle_position);
    //         std::cout << "Obstacle position AFTER: " << Eigen::Vector3d(b_env->obstacle_models[i]->getT()).transpose() << std::endl;
    //     }
    //     b->SetBurEnv(b_env);
    //     b_env->SetPoses(config);
    //     // add obstacles to b_env
    //     // add robot objects to b_env
    //     // freeze
    //     // load the forward kinematics functions:
    //     //  1. forwardRt(q)
    //     //  2. forward(i,q)
    //     //  3. r(i, q1, q2, t)

    //     // Eigen::MatrixXd a = MatrixXd::Zero(2, 1);
    //     // std::cout << std::endl
    //     //           << "TESTING BUR: " << std::endl;
    //     // Bur bur = b->GetBur(config, config2, 0.5);
    //     // std::cout << bur;
    //     // std::cout << "END TESTING BUR" << std::endl
    //     //           << std::endl;
    //     // std::cout << "START CHECK: CLOSEST DISTANCE: " << b->GetClosestDistance(config) << std::endl;
    //     // std::exit(1);
    //     std::cout << "CLOSEST DISTANCE TEST" << std::endl;
    //     double closest_distance_test = b->GetClosestDistance(Vector2d(0, 0));
    //     std::cout << "distance: " << closest_distance_test << std::endl;
    //     std::cout << "END CLOSEST DISTANCE TEST" << std::endl;
    //     std::cout << std::endl;

    //     std::cout << "TEST RBTCONNECT" << std::endl;
    //     // std::cout << "START CHECK: CLOSEST DISTANCE: " << b->GetClosestDistance(config) << std::endl;
    //     std::optional<std::vector<Eigen::VectorXd>> path = b->RbtConnect(config, config2);
    //     if (!path)
    //     {
    //         std::cout << "test_forward: path not found" << std::endl;
    //         return;
    //     }
    //     std::vector<Eigen::VectorXd> myPath = path.value();
    //     std::cout << "Path: " << std::endl;
    //     for (int i = 0; i < myPath.size(); ++i)
    //     {
    //         std::cout << myPath[i].transpose() << std::endl;
    //     }
    //     std::ofstream myFileA;

    //     myFileA.open("path.txt");
    //     std::string robot_name = argv[1];
    //     std::string obstacle_name = argv[2];
    //     myFileA << "robot," << robot_name << "," << std::endl;
    //     myFileA << "obstacle," << obstacle_name << "," << std::endl;
    //     myFileA << "obstacle_position," << std::endl;
    //     for (int i = 0; i < 3; ++i)
    //     {
    //         myFileA << obstacle_position(i) << ",";
    //     }
    //     myFileA << std::endl;

    //     // std::cout
    //     //     << "Cols: " << myPath.cols() << "  Rows: " << myPath.rows() << std::endl;

    //     myFileA << "path," << std::endl;
    //     for (int i = 0; i < myPath.size(); i++)
    //     {
    //         VectorXd result = myPath[i];
    //         for (int k = 0; k < result.size(); ++k)
    //         {
    //             myFileA << result(k) << ",";
    //         }
    //         myFileA << std::endl;
    //     }
    //     myFileA.close();

    //     std::cout << std::endl;

    //     for (int i = 0; i < myPath.size(); i++)
    //     {
    //         std::cout << "step " << i << ": " << myPath[i].transpose() << " delta distance [m]: " << fk(1, myPath[i]).transpose() << std::endl;
    //     }
    //     std::cout << "END TEST RBTCONNECT" << std::endl;
    //     std::cout << std::endl;

    //     std::cout << "BEGIN TEST URDF" << std::endl;
    //     if (argc > 6)
    //     {
    //         test_urdf(argv[6]);
    //     }
    //     std::cout << "END TEST URDF" << std::endl;
    //     std::cout << std::endl;
    // }
}