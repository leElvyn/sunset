//
// Created by red on 5/28/26.
//

#ifndef TP2_OBJECTS_H
#define TP2_OBJECTS_H
#include <vector>
#include <glm/vec3.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/trigonometric.hpp>

struct vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct triangle {
    vertex v1;
    vertex v2;
    vertex v3;
};

class objects {
};

class Cylinder {
public:
    Cylinder(const glm::vec3 &base_center, float base_radius, float height)
        : base_center(base_center),
          base_radius(base_radius),
          height(height) {
    }

    glm::vec3 base_center;
    float base_radius;
    float height;

    std::vector<triangle> to_mesh(int divisions) {
        float pi = glm::pi<float>() * 2;
        float arc = pi / divisions;
        std::vector<vertex> bottom_circle_points;
        std::vector<vertex> top_circle_points;
        for (int i = 0; i < divisions; ++i) {
            glm::vec3 normal = glm::vec3(glm::cos(arc * ((float)i + 0.5) ), glm::sin(arc * ((float)i  + 0.5)), 0);
            bottom_circle_points.push_back(vertex {glm::vec3(base_radius* glm::cos(arc * (float)i), base_radius *glm::sin(arc * (float)i), 0), normal});
            top_circle_points.push_back(vertex {glm::vec3(base_radius* glm::cos(arc * (float)i), base_radius *glm::sin(arc * (float)i), height), normal});
        }
        std::vector<triangle> triangles;

        for (int i = 0; i < divisions - 1; ++i) {
            triangles.push_back(triangle { bottom_circle_points[i], top_circle_points[i+1], top_circle_points[i] });
            triangles.push_back(triangle {bottom_circle_points[i] , bottom_circle_points[i+1], top_circle_points[i+1] });

        }
        triangles.push_back(triangle { bottom_circle_points[divisions - 1], top_circle_points[0], top_circle_points[divisions - 1] });
        triangles.push_back(triangle {bottom_circle_points[divisions - 1] , bottom_circle_points[0], top_circle_points[0] });

        return triangles;

    }
};


#endif //TP2_OBJECTS_H
