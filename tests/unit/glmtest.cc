#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iomanip>
#include <tests.hh>

namespace GlmTests {
    using namespace testing;
    using namespace std;

    void GlmTest() {
        glm::vec3 startDir(0, 1, 0);
        float angle = 0.0004f;
        glm::vec3 endDir = glm::vec3(0, cos(angle), sin(angle));

        cout << setprecision(10);

        auto axis = glm::normalize(glm::cross(startDir, endDir));
        cout << "Input: " << angle << " x (" << axis.x << ", " << axis.y << ", " << axis.z << ")" << endl;

        auto deltaRotation = glm::angleAxis(angle, axis);
        auto axis1 = glm::axis(deltaRotation);
        cout << glm::angle(deltaRotation) << " x (" << axis1.x << ", " << axis1.y << ", " << axis1.z << ")" << endl;

        auto dir = deltaRotation * startDir;
        cout << "(" << endDir.x << ", " << endDir.y << ", " << endDir.z << ")";
        cout << " -> (" << dir.x << ", " << dir.y << ", " << dir.z << ")" << endl;

        auto deltaRotation2 = glm::rotation(startDir, endDir);
        auto axis2 = glm::axis(deltaRotation2);
        cout << glm::angle(deltaRotation2) << " x (" << axis2.x << ", " << axis2.y << ", " << axis2.z << ")" << endl;

        auto dir2 = deltaRotation2 * startDir;
        cout << "(" << endDir.x << ", " << endDir.y << ", " << endDir.z << ")";
        cout << " -> (" << dir2.x << ", " << dir2.y << ", " << dir2.z << ")" << endl;
    }

    // Register the test
    Test test(&GlmTest);
} // namespace GlmTests
