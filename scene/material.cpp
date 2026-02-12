#include "material.h"
#include "../ui/TraceUI.h"
#include "light.h"
#include "ray.h"
extern TraceUI *traceUI;

#include "../fileio/images.h"
#include <glm/gtx/io.hpp>
#include <iostream>

using namespace std;
extern bool debugMode;

Material::~Material() {}

// Apply the phong model to this point on the surface of the object, returning
// the color of that point.
glm::dvec3 Material::shade(Scene *scene, const ray &r, const isect &i) const {
  // YOUR CODE HERE

  // For now, this method just returns the diffuse color of the object.
  // This gives a single matte color for every distinct surface in the
  // scene, and that's it.  Simple, but enough to get you started.
  // (It's also inconsistent with the phong model...)

  // Your mission is to fill in this method with the rest of the phong
  // shading model, including the contributions of all the light sources.
  // You will need to call both distanceAttenuation() and
  // shadowAttenuation()
  // somewhere in your code in order to compute shadows and light falloff.
  //	if( debugMode )
  //		std::cout << "Debugging Phong code..." << std::endl;

  // When you're iterating through the lights,
  // you'll want to use code that looks something
  // like this:
  //
  // for ( const auto& pLight : scene->getAllLights() )
  // {
  //              // pLight has type Light*
  // 		.
  // 		.
  // 		.
  // }
  
  // Initialize with emissive and ambient components
  glm::dvec3 result = ke(i) + ka(i) * scene->ambient();
  
  // Get intersection point and normal
  glm::dvec3 Q = r.at(i);
  glm::dvec3 N = i.getN();
  glm::dvec3 d = glm::normalize(-r.getDirection());  // View direction
  
  // Iterate through all light sources
  for (const auto& pLight : scene->getAllLights()) {
    // Calculate attenuation (distance + shadow)
    glm::dvec3 atten = pLight->distanceAttenuation(Q) * 
                       pLight->shadowAttenuation(r, Q);
    
    // Light direction
    glm::dvec3 L = glm::normalize(pLight->getDirection(Q));
    
    // Diffuse component: kd * (N · L)
    glm::dvec3 diffuse = kd(i) * pLight->getColor() * 
                         glm::max(0.0, glm::dot(N, L));
    
    // Specular component: ks * (R · V)^n
    glm::dvec3 R = glm::reflect(-L, N);
    glm::dvec3 specular = ks(i) * pLight->getColor() * 
                          glm::pow(glm::max(0.0, glm::dot(R, d)), 
                                   shininess(i));
    
    // Add attenuated light contribution
    result += atten * (diffuse + specular);
  }
  
  return result;
}

TextureMap::TextureMap(string filename) {
  data = readImage(filename.c_str(), width, height);
  if (data.empty()) {
    width = 0;
    height = 0;
    string error("Unable to load texture map '");
    error.append(filename);
    error.append("'.");
    throw TextureMapException(error);
  }
}

glm::dvec3 TextureMap::getMappedValue(const glm::dvec2 &coord) const {
  // YOUR CODE HERE
  //
  // In order to add texture mapping support to the
  // raytracer, you need to implement this function.
  // What this function should do is convert from
  // parametric space which is the unit square
  // [0, 1] x [0, 1] in 2-space to bitmap coordinates,
  // and use these to perform bilinear interpolation
  // of the values.
  
  // Convert from parametric space [0, 1] x [0, 1] to bitmap coordinates
  double u = coord.x * (width - 1);
  double v = coord.y * (height - 1);
  
  // Get integer coordinates
  int x0 = (int)glm::floor(u);
  int y0 = (int)glm::floor(v);
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  
  // Get fractional parts for interpolation
  double fu = u - x0;
  double fv = v - y0;
  
  // Get the four corner pixels
  glm::dvec3 p00 = getPixelAt(x0, y0);
  glm::dvec3 p10 = getPixelAt(x1, y0);
  glm::dvec3 p01 = getPixelAt(x0, y1);
  glm::dvec3 p11 = getPixelAt(x1, y1);
  
  // Bilinear interpolation
  glm::dvec3 p0 = glm::mix(p00, p10, fu);
  glm::dvec3 p1 = glm::mix(p01, p11, fu);
  glm::dvec3 result = glm::mix(p0, p1, fv);
  
  return result;
}

glm::dvec3 TextureMap::getPixelAt(int x, int y) const {
  // YOUR CODE HERE:
  
  // Clamp coordinates to valid range [0, width-1] and [0, height-1]
  x = glm::clamp(x, 0, width - 1);
  y = glm::clamp(y, 0, height - 1);
  
  // Calculate the index in the flat data array
  // Assuming data is stored as RGB values (3 components per pixel)
  int index = (y * width + x) * 3;
  
  // Extract RGB values and normalize to [0, 1] range
  glm::dvec3 pixel(
    data[index] / 255.0,
    data[index + 1] / 255.0,
    data[index + 2] / 255.0
  );
  
  return pixel;
}

glm::dvec3 MaterialParameter::value(const isect &is) const {
  if (0 != _textureMap)
    return _textureMap->getMappedValue(is.getUVCoordinates());
  else
    return _value;
}

double MaterialParameter::intensityValue(const isect &is) const {
  if (0 != _textureMap) {
    glm::dvec3 value(_textureMap->getMappedValue(is.getUVCoordinates()));
    return (0.299 * value[0]) + (0.587 * value[1]) + (0.114 * value[2]);
  } else
    return (0.299 * _value[0]) + (0.587 * _value[1]) + (0.114 * _value[2]);
}
