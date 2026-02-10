#include <cmath>
#include <iostream>

#include "light.h"
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>

using namespace std;

double DirectionalLight::distanceAttenuation(const glm::dvec3 &) const {
  // distance to light is infinite, so f(di) goes to 0.  Return 1.
  return 1.0;
}

glm::dvec3 DirectionalLight::shadowAttenuation(const ray &r, const glm::dvec3 &p) const {
  //YOUR CODE HERE:
  
  // Direction towards the light source (opposite of orientation)
  glm::dvec3 d = -orientation;
  
  // Cast a ray from point p towards the light
  ray shadowRay(p, d, glm::dvec3(1.0, 1.0, 1.0), ray::VISIBILITY);
  
  // Find intersection with scene
  isect i;
  if (scene->intersect(shadowRay, i)) {
    // If we hit something, the point is in shadow
    return glm::dvec3(0, 0, 0);
  }
  
  // Point is not in shadow
  return glm::dvec3(1, 1, 1);
}

glm::dvec3 DirectionalLight::getColor() const { return color; }

glm::dvec3 DirectionalLight::getDirection(const glm::dvec3 &) const {
  return -orientation;
}

double PointLight::distanceAttenuation(const glm::dvec3 &P) const {
  // YOUR CODE HERE:
  // Calculate distance from light source to point P
  double distance = glm::length(position - P);
  
  // Inverse square law attenuation: 1 / (distance^2)
  // Add small epsilon to avoid division by zero
  return 1.0 / (distance * distance + 1e-6);
}

glm::dvec3 PointLight::getColor() const { return color; }

glm::dvec3 PointLight::getDirection(const glm::dvec3 &P) const {
  return glm::normalize(position - P);
}

glm::dvec3 PointLight::shadowAttenuation(const ray &r, const glm::dvec3 &p) const {
  // YOUR CODE HERE:
  // You should implement shadow-handling code here.
  
  // Direction from P to the light source
  glm::dvec3 d = glm::normalize(position - p);
  
  // Cast a ray from point p towards the light
  ray shadowRay(p, d, glm::dvec3(1.0, 1.0, 1.0), ray::VISIBILITY);
  
  // Find intersection with scene
  isect i;
  if (scene->intersect(shadowRay, i)) {
    // The intersection point Q is at: p + i.t * d
    // Check if Q is before the light source
    double distToLight = glm::length(position - p);
    
    if (i.getT() < distToLight) {
      // Point is in shadow
      return glm::dvec3(0, 0, 0);
    }
  }
  
  // Point is not in shadow
  return glm::dvec3(1, 1, 1);
}

#define VERBOSE 0
