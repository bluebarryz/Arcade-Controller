#include <gtest/gtest.h>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include "arcade_control/MotorDriver.hpp"
#include "arcade_control/srv/joystick_input.hpp"
#include "arcade_control/msg/speed.hpp"
#include <chrono>

using namespace std::chrono_literals;

class TestMotorDriver : public ::testing::Test {
protected:
    void SetUp() override {
        rclcpp::init(0, nullptr);
        test_node = std::make_shared<rclcpp::Node>("test_node");
        
        motor_driver = std::make_shared<composition::MotorDriver>(rclcpp::NodeOptions());

        last_speed_msg = nullptr;
        speed_sub = test_node->create_subscription<arcade_control::msg::Speed>(
            "/cmd_vel",
            10,
            [this](const arcade_control::msg::Speed::SharedPtr msg) {
                std::cout << "received msg on sub! " << msg->r << std::endl;
                last_speed_msg = msg;
            });

        client = test_node->create_client<arcade_control::srv::JoystickInput>("/joystick_input");
        std::cout << "Setup completed" << std::endl;
    }

    void TearDown() override {
        motor_driver.reset();
        test_node.reset();
        rclcpp::shutdown();
        std::cout << "Teardown complete" << std::endl;
    }

    std::shared_ptr<rclcpp::Node> test_node;
    std::shared_ptr<composition::MotorDriver> motor_driver;
    rclcpp::Client<arcade_control::srv::JoystickInput>::SharedPtr client;
    rclcpp::Subscription<arcade_control::msg::Speed>::SharedPtr speed_sub;
    arcade_control::msg::Speed::SharedPtr last_speed_msg;

    bool SendJoystickInput(float rotate, float drive) {
        auto request = std::make_shared<arcade_control::srv::JoystickInput::Request>();
        request->x = rotate;
        request->y = drive;

        if (!client->wait_for_service(1s)) {
            RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Interrupted while waiting for the service. Exiting.");
            rclcpp::shutdown();
            return false;
        }

        std::cout << "make request" << std::endl;
        auto result = client->async_send_request(request);
        std::cout << "make request!" << std::endl;

        auto executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
        executor->add_node(test_node);
        executor->add_node(motor_driver);

        // Wait for result
        if (executor->spin_until_future_complete(result) ==
            rclcpp::FutureReturnCode::SUCCESS) {
            RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "successful request!");
        } else {
            RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Failed to call service");
            rclcpp::shutdown();
            return false;
        }

        return result.get()->success;
    }
    
    // Wait for the /cmd_vel subscriber to receive the expected speed values
    bool WaitForSpeedMessage(float expected_r, float expected_l, const std::chrono::seconds timeout = std::chrono::seconds(3)) {
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            rclcpp::spin_some(test_node);
            rclcpp::spin_some(motor_driver);
            
            if (last_speed_msg != nullptr) {
                RCLCPP_INFO(rclcpp::get_logger("TestMotorDriver"), "Received speed message: r=%.2f, l=%.2f", last_speed_msg->r, last_speed_msg->l);
                
                if (std::abs(last_speed_msg->r - expected_r) < 1e-6 && std::abs(last_speed_msg->l - expected_l) < 1e-6) {
                    return true;
                }
            }
            
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                RCLCPP_ERROR(rclcpp::get_logger("TestMotorDriver"), "Timed out waiting for expected speed message");
                return false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void assert_motor_speed(float joystick_rotate, float joystick_drive, float expected_l, float expected_r) {
        ASSERT_TRUE(SendJoystickInput(joystick_rotate, joystick_drive));
        ASSERT_TRUE(WaitForSpeedMessage(expected_l, expected_r));
        ASSERT_NE(last_speed_msg, nullptr);
        
        EXPECT_FLOAT_EQ(last_speed_msg->l, expected_l);
        EXPECT_FLOAT_EQ(last_speed_msg->r, expected_r);
    }
};

TEST_F(TestMotorDriver, Test1) {
    std::cout << "Test1" <<std::endl;
    
    float joystick_rotate = 0.0;
    float joystick_drive = 1.0;
    float expected_l = 1.0;
    float expected_r = 1.0;
    assert_motor_speed(joystick_rotate, joystick_drive, expected_l, expected_r);
    
}

TEST_F(TestMotorDriver, Test2) {
    float joystick_rotate = 0.0;
    float joystick_drive = 0 - 1.0;
    float expected_l = 0 - 1.0;
    float expected_r = 0 - 1.0;
    assert_motor_speed(joystick_rotate, joystick_drive, expected_l, expected_r);
}

// TEST_F(TestMotorDriver, Test3) {
//     float joystick_rotate_0 = 0.5;
//     float joystick_drive_0 = 0.5;
//     float expected_l_0 = 0.5;
//     float expected_r_0 = 0;
//     assert_motor_speed(joystick_rotate_0, joystick_drive_0, expected_l_0, expected_r_0);

//     float joystick_rotate = 0.01;
//     float joystick_drive = 0 - 0.05;
//     assert_motor_speed(joystick_rotate, joystick_drive, expected_l_0, expected_r_0);
// }

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}