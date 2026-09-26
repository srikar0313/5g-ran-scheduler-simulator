#include "ran/project_info.hpp"

#include <gtest/gtest.h>

TEST(ProjectInfoTest, ReturnsProjectName) {
  EXPECT_EQ(ran::projectName(), "5G RAN Scheduling and Test Automation Simulator");
}
