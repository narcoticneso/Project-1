#include "trimesh.h"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <float.h>
#include <string.h>
#include "../ui/TraceUI.h"

extern TraceUI *traceUI;

using namespace std;

Trimesh::~Trimesh() {
  for (auto f : faces)
    delete f;
}

// must add vertices, normals, and materials IN ORDER
void Trimesh::addVertex(const glm::dvec3 &v) { vertices.emplace_back(v); }

void Trimesh::addNormal(const glm::dvec3 &n) { normals.emplace_back(n); }

void Trimesh::addColor(const glm::dvec3 &c) { vertColors.emplace_back(c); }

void Trimesh::addUV(const glm::dvec2 &uv) { uvCoords.emplace_back(uv); }

// Returns false if the vertices a,b,c don't all exist
bool Trimesh::addFace(int a, int b, int c) {
  int vcnt = vertices.size();

  if (a >= vcnt || b >= vcnt || c >= vcnt)
    return false;

  TrimeshFace *newFace = new TrimeshFace(this, a, b, c);
  if (!newFace->degen)
    faces.push_back(newFace);
  else
    delete newFace;

  // Don't add faces to the scene's object list so we can cull by bounding
  // box
  return true;
}

// Check to make sure that if we have per-vertex materials or normals
// they are the right number.
const char *Trimesh::doubleCheck() {
  if (!vertColors.empty() && vertColors.size() != vertices.size())
    return "Bad Trimesh: Wrong number of vertex colors.";
  if (!uvCoords.empty() && uvCoords.size() != vertices.size())
    return "Bad Trimesh: Wrong number of UV coordinates.";
  if (!normals.empty() && normals.size() != vertices.size())
    return "Bad Trimesh: Wrong number of normals.";

  return 0;
}

bool Trimesh::intersectLocal(ray &r, isect &i) const {
  bool have_one = false;
  for (auto face : faces) {
    isect cur;
    if (face->intersectLocal(r, cur)) {
      if (!have_one || (cur.getT() < i.getT())) {
        i = cur;
        have_one = true;
      }
    }
  }
  if (!have_one)
    i.setT(1000.0);
  return have_one;
}

bool TrimeshFace::intersect(ray &r, isect &i) const {
  return intersectLocal(r, i);
}

// Intersect ray r with the triangle. If it hits returns true,
// and fill in the intersection record i.
bool TrimeshFace::intersectLocal(ray &r, isect &i) const {
  const double EPS = 1e-9;

  int ia = (*this)[0];
  int ib = (*this)[1];
  int ic = (*this)[2];

  const glm::dvec3 &A = parent->vertices[ia];
  const glm::dvec3 &B = parent->vertices[ib];
  const glm::dvec3 &C = parent->vertices[ic];

  const glm::dvec3 O = r.getPosition();
  const glm::dvec3 D = r.getDirection();

  // Möller–Trumbore intersection
  glm::dvec3 e1 = B - A;
  glm::dvec3 e2 = C - A;

  glm::dvec3 pvec = glm::cross(D, e2);
  double det = glm::dot(e1, pvec);

  if (std::fabs(det) < EPS)
    return false;

  double invDet = 1.0 / det;

  glm::dvec3 tvec = O - A;
  double u = glm::dot(tvec, pvec) * invDet;
  if (u < 0.0 || u > 1.0)
    return false;

  glm::dvec3 qvec = glm::cross(tvec, e1);
  double v = glm::dot(D, qvec) * invDet;
  if (v < 0.0 || (u + v) > 1.0)
    return false;

  double t = glm::dot(e2, qvec) * invDet;
  if (t <= EPS)
    return false;

  // Hit!
  i.setT(t);
  i.setObject(this->parent);

  double w = 1.0 - u - v;

  // Normal: interpolate vertex normals if present, else face normal
  glm::dvec3 N;
  if (parent->vertNorms && !parent->normals.empty()) {
    const glm::dvec3 &nA = parent->normals[ia];
    const glm::dvec3 &nB = parent->normals[ib];
    const glm::dvec3 &nC = parent->normals[ic];
    N = glm::normalize(w * nA + u * nB + v * nC);
  } else {
    N = glm::normalize(glm::cross(e1, e2));
  }
  i.setN(N);

  /* To determine the color of an intersection, use the following rules:
     - If the parent mesh has non-empty `uvCoords`, barycentrically interpolate
       the UV coordinates of the three vertices of the face, then assign it to
       the intersection using i.setUVCoordinates().
     - Otherwise, if the parent mesh has non-empty `vertexColors`,
       barycentrically interpolate the colors from the three vertices of the
       face. Create a new material by copying the parent's material, set the
       diffuse color of this material to the interpolated color, and then
       assign this material to the intersection.
     - If neither is true, assign the parent's material to the intersection.
  */

  if (!parent->uvCoords.empty()) {
    const glm::dvec2 &uvA = parent->uvCoords[ia];
    const glm::dvec2 &uvB = parent->uvCoords[ib];
    const glm::dvec2 &uvC = parent->uvCoords[ic];
    glm::dvec2 uv = w * uvA + u * uvB + v * uvC;
    i.setUVCoordinates(uv);

    i.setMaterial(parent->material);
  }
  else if (!parent->vertColors.empty()) {
    const glm::dvec3 &cA = parent->vertColors[ia];
    const glm::dvec3 &cB = parent->vertColors[ib];
    const glm::dvec3 &cC = parent->vertColors[ic];
    glm::dvec3 col = w * cA + u * cB + v * cC;

    Material m = parent->material;
    m.setDiffuse(col);
    i.setMaterial(m);
  }
  else {
    i.setMaterial(parent->material);
  }

  return true;
}

// Once all the verts and faces are loaded, per vertex normals can be
// generated by averaging the normals of the neighboring faces.
void Trimesh::generateNormals() {
  int cnt = vertices.size();
  normals.resize(cnt);
  std::vector<int> numFaces(cnt, 0);

  for (auto face : faces) {
    glm::dvec3 faceNormal = face->getNormal();

    for (int i = 0; i < 3; ++i) {
      normals[(*face)[i]] += faceNormal;
      ++numFaces[(*face)[i]];
    }
  }

  for (int i = 0; i < cnt; ++i) {
    if (numFaces[i])
      normals[i] /= numFaces[i];
  }

  vertNorms = true;
}