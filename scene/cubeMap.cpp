#include "cubeMap.h"
#include "../scene/material.h"
#include "../ui/TraceUI.h"
#include "ray.h"
extern TraceUI *traceUI;

glm::dvec3 CubeMap::getColor(ray r) const {
  // YOUR CODE HERE
  // FIXME: Implement Cube Map here

  //Get the normalized direction vector for the ray
  glm::dvec3 rdir = normalize(r.getDirection());

  //Get the absolute value of the direction vector
  glm::dvec3 abs_rdir = glm::abs(rdir);

  //Initialize u and v
  double u = 0.0;
  double v = 0.0;

  //Separate into components
  double abs_rdirx = abs_rdir.x;
  double abs_rdiry = abs_rdir.y;
  double abs_rdirz = abs_rdir.z;

  //xpos = 0, xneg = 1, ypos = 2, yneg = 3, zpos = 4, zneg = 5
  int face = -1;

  //Determine along which axis the direction is moving most
  //If x is greatest:
  if (abs_rdirx >= abs_rdiry && abs_rdirx >= abs_rdirz)
  {
    //X positive
    if(rdir.x >= 0) {
      face = 0;
    } 
    //X negative
    else{
      face = 1;
    }
    u = -rdir.z / abs_rdirx;
    v = rdir.y / abs_rdirx;
  }
  //If y is the greatest
  else if (abs_rdiry >= abs_rdirz)
  {
    //y positive
    if(rdir.y >=0){
      face = 2;
    }
    //y negative
    else{
      face = 3;
    }
    u = rdir.x / abs_rdiry;
    v = -rdir.z / abs_rdiry;
  }
  else
  {
    //z positive
    if(rdir.z >= 0){
      face = 4;
    }
    //z negative
    else{
      face = 5;
    }
    u = rdir.x / abs_rdirz;
    v = rdir.y / abs_rdirz;
  }
  //Fit to unit square
  u = 0.5 * (u + 1);
  v = 0.5 * (v+1);

  //Ensure it stays within unit square 
  double margin = 1e-6;
  u = glm::clamp(u, 0.0, 1.0-margin);
  v = glm::clamp(v, 0.0, 1.0-margin);

  //Account for occasions where tMap[face] returns a null pointer
  if (tMap[face] == nullptr)
  {
    return glm::dvec3(0.0, 0.0, 0.0);
  }
  return tMap[face]->getMappedValue(glm::dvec2(u,v));
}

CubeMap::CubeMap() {}

CubeMap::~CubeMap() {}

void CubeMap::setNthMap(int n, TextureMap *m) {
  if (m != tMap[n].get())
    tMap[n].reset(m);
}
