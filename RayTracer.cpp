// The main ray tracer.

#pragma warning(disable : 4786)

#include "RayTracer.h"
#include "scene/light.h"
#include "scene/material.h"
#include "scene/ray.h"

#include "parser/JsonParser.h"
#include "parser/Parser.h"
#include "parser/Tokenizer.h"
#include <json.hpp>

#include "ui/TraceUI.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>
#include <string.h> // for memset

#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;
extern TraceUI *traceUI;

// Use this variable to decide if you want to print out debugging messages. Gets
// set in the "trace single ray" mode in TraceGLWindow, for example.
bool debugMode = false;

// Trace a top-level ray through pixel(i,j), i.e. normalized window coordinates
// (x,y), through the projection plane, and out into the scene. All we do is
// enter the main ray-tracing method, getting things started by plugging in an
// initial ray weight of (0.0,0.0,0.0) and an initial recursion depth of 0.

glm::dvec3 RayTracer::trace(double x, double y) {
  // Clear out the ray cache in the scene for debugging purposes,
  if (TraceUI::m_debug) {
    scene->clearIntersectCache();
  }

  ray r(glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 0), glm::dvec3(1, 1, 1),
        ray::VISIBILITY);
  scene->getCamera().rayThrough(x, y, r);
  double dummy;
  glm::dvec3 ret =
      traceRay(r, glm::dvec3(1.0, 1.0, 1.0), traceUI->getDepth(), dummy);
  ret = glm::clamp(ret, 0.0, 1.0);
  return ret;
}

glm::dvec3 RayTracer::tracePixel(int i, int j) {
  glm::dvec3 col(0, 0, 0);

  if (!sceneLoaded())
    return col;

  double x = double(i) / double(buffer_width);
  double y = double(j) / double(buffer_height);

  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;
  col = trace(x, y);

  pixel[0] = (int)(255.0 * col[0]);
  pixel[1] = (int)(255.0 * col[1]);
  pixel[2] = (int)(255.0 * col[2]);
  return col;
}

#define VERBOSE 0

// Do recursive ray tracing! You'll want to insert a lot of code here (or places
// called from here) to handle reflection, refraction, etc etc.
glm::dvec3 RayTracer::traceRay(ray &r, const glm::dvec3 &thresh, int depth,
                               double &t) {
  isect i;
  glm::dvec3 colorC;
#if VERBOSE
  std::cerr << "== current depth: " << depth << std::endl;
#endif

  if (scene->intersect(r, i)) {
    // Get the material for the surface
    const Material &m = i.getMaterial();
    
    // Get intersection point Q and normal N
    glm::dvec3 Q = r.at(i);
    glm::dvec3 N = i.getN();
    glm::dvec3 d = r.getDirection();
    
    // Base shading (diffuse + specular + shadows)
    colorC = m.shade(scene.get(), r, i);
    
    // Recursion depth check
    if (depth > 0) {
      // Reflection
      if (m.kr(i).length() > 0.0) {
        glm::dvec3 R = glm::reflect(d, N);
        ray reflectRay(Q, R, glm::dvec3(1.0, 1.0, 1.0), ray::VISIBILITY);
        double tReflect;
        glm::dvec3 reflectColor = traceRay(reflectRay, thresh, depth - 1, tReflect);
        colorC += m.kr(i) * reflectColor;
      }
      
      // Refraction
      if (m.kt(i).length() > 0.0) {
        // Determine refractive indices
        double n_i, n_t;
        
        // Check if ray is entering or exiting the object
        double cosIncident = glm::dot(d, N);
        
        if (cosIncident > 0.0) {  // Ray is exiting object
          n_i = m.index(i);
          n_t = 1.0;  // Index of air
          N = -N; // Flip normal to face the ray direction
        } else {  // Ray is entering object
          n_i = 1.0;  // Index of air
          n_t = m.index(i);
        }
        
        // Check for total internal reflection
        double cosThetaI = -glm::dot(d, N);
        double sinThetaT2 = (n_i / n_t) * (n_i / n_t) * (1.0 - cosThetaI * cosThetaI);
        
        if (sinThetaT2 < 1.0) {  // No total internal reflection
          double cosThetaT = glm::sqrt(1.0 - sinThetaT2);
          glm::dvec3 T = (n_i / n_t) * (-d) + 
                         ((n_i / n_t) * cosThetaI - cosThetaT) * N;
          T = glm::normalize(T);
          
          ray refractRay(Q, T, glm::dvec3(1.0, 1.0, 1.0), ray::VISIBILITY);
          double tRefract;
          glm::dvec3 refractColor = traceRay(refractRay, thresh, depth - 1, tRefract);
          colorC += m.kt(i) * refractColor;
        }
      }
    }
  } else {
    // No intersection - return background color
    colorC = glm::dvec3(0.0, 0.0, 0.0);

    // added: check to see if cubemap is loaded, then get color
    if(traceUI->cubeMap()){
      CubeMap *cm = traceUI->getCubeMap();
      colorC = cm->getColor(r);
    }
  }
#if VERBOSE
  std::cerr << "== depth: " << depth << " done, returning: " << colorC
            << std::endl;
#endif
  return colorC;
}

RayTracer::RayTracer()
    : scene(nullptr), buffer(0), thresh(0), buffer_width(0), buffer_height(0),
      m_bBufferReady(false) {
}

RayTracer::~RayTracer() {}

void RayTracer::getBuffer(unsigned char *&buf, int &w, int &h) {
  buf = buffer.data();
  w = buffer_width;
  h = buffer_height;
}

double RayTracer::aspectRatio() {
  return sceneLoaded() ? scene->getCamera().getAspectRatio() : 1;
}

bool RayTracer::loadScene(const char *fn) {
  ifstream ifs(fn);
  if (!ifs) {
    string msg("Error: couldn't read scene file ");
    msg.append(fn);
    traceUI->alert(msg);
    return false;
  }

  // Check if fn ends in '.ray'
  bool isRay = false;
  const char *ext = strrchr(fn, '.');
  if (ext && !strcmp(ext, ".ray"))
    isRay = true;

  // Strip off filename, leaving only the path:
  string path(fn);
  if (path.find_last_of("\\/") == string::npos)
    path = ".";
  else
    path = path.substr(0, path.find_last_of("\\/"));

  if (isRay) {
    // .ray Parsing Path
    // Call this with 'true' for debug output from the tokenizer
    Tokenizer tokenizer(ifs, false);
    Parser parser(tokenizer, path);
    try {
      scene.reset(parser.parseScene());
    } catch (SyntaxErrorException &pe) {
      traceUI->alert(pe.formattedMessage());
      return false;
    } catch (ParserException &pe) {
      string msg("Parser: fatal exception ");
      msg.append(pe.message());
      traceUI->alert(msg);
      return false;
    } catch (TextureMapException e) {
      string msg("Texture mapping exception: ");
      msg.append(e.message());
      traceUI->alert(msg);
      return false;
    }
  } else {
    // JSON Parsing Path
    try {
      JsonParser parser(path, ifs);
      scene.reset(parser.parseScene());
    } catch (ParserException &pe) {
      string msg("Parser: fatal exception ");
      msg.append(pe.message());
      traceUI->alert(msg);
      return false;
    } catch (const json::exception &je) {
      string msg("Invalid JSON encountered ");
      msg.append(je.what());
      traceUI->alert(msg);
      return false;
    }
  }

  if (!sceneLoaded())
    return false;

  return true;
}

void RayTracer::traceSetup(int w, int h) {
  size_t newBufferSize = w * h * 3;
  if (newBufferSize != buffer.size()) {
    bufferSize = newBufferSize;
    buffer.resize(bufferSize);
  }
  buffer_width = w;
  buffer_height = h;
  std::fill(buffer.begin(), buffer.end(), 0);
  m_bBufferReady = true;

  /*
   * Sync with TraceUI
   */

  threads = traceUI->getThreads();
  block_size = traceUI->getBlockSize();
  thresh = traceUI->getThreshold();
  samples = traceUI->getSuperSamples();
  aaThresh = traceUI->getAaThreshold();

  // YOUR CODE HERE
  // FIXME: Additional initializations
}

/*
 * RayTracer::traceImage
 *
 *	Trace the image and store the pixel data in RayTracer::buffer.
 *  
 *	Arguments:
 *		w:	width of the image buffer
 *		h:	height of the image buffer
 *
 */
void RayTracer::traceImage(int w, int h) {
  // Always call traceSetup before rendering anything.
  traceSetup(w, h);

  // Get the number of threads to use
  int numThreads = traceUI->getThreads();

  // Resize and initialize the threadStatus array
  threadStatus.resize(numThreads);
  for (int t = 0; t < numThreads; t++) {
    threadStatus[t] = false; // Mark all threads as not finished
  }

  // Lambda function for rendering a portion of the image
  auto renderChunk = [this](int startCol, int endCol, int startRow, int endRow, int threadIndex) {
    for (int j = startRow; j < endRow; j++) {
      for (int i = startCol; i < endCol; i++) {
        // Convert pixel (i, j) to normalized window coordinates (NDC)
        double x = double(i) / double(buffer_width);
        double y = double(j) / double(buffer_height);

        // Construct the ray explicitly
        ray r(glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 0), glm::dvec3(1.0, 1.0, 1.0), ray::VISIBILITY);

        // Use rayThrough to calculate the ray's origin and direction
        scene->getCamera().rayThrough(x, y, r);

        // Trace the ray and get the color
        double t;
        glm::dvec3 color = traceRay(r, glm::dvec3(1.0, 1.0, 1.0), traceUI->getDepth(), t);

        // Set the pixel color in the buffer
        setPixel(i, j, color);
      }
    }

    // Mark this thread as finished
    threadStatus[threadIndex] = true;
  };

  // Clear any existing worker threads
  workerThreads.clear();

  // Divide the image into vertical strips (each thread renders a different section horizontally)
  int colsPerThread = w / numThreads;
  for (int t = 0; t < numThreads; t++) {
    int startCol = t * colsPerThread;
    int endCol = (t == numThreads - 1) ? w : startCol + colsPerThread;
    int startRow = 0;
    int endRow = h;
    
    workerThreads.emplace_back(renderChunk, startCol, endCol, startRow, endRow, t);
  }
}

int RayTracer::aaImage() {
  // YOUR CODE HERE
  // FIXME: Implement Anti-aliasing here
  //
  // TIP: samples and aaThresh have been synchronized with TraceUI by
  //      RayTracer::traceSetup() function
  
  if (!sceneLoaded()) {
    return 0; // No scene loaded, nothing to do
  }

  // Number of supersamples per pixel (e.g., 4 for 2x2 sampling)
  int numSamples = samples; // `samples` is synchronized with TraceUI

  // Iterate over each pixel in the image
  for (int j = 0; j < buffer_height; j++) {
    for (int i = 0; i < buffer_width; i++) {
      glm::dvec3 color(0, 0, 0);

      // Supersampling: Take multiple samples within the pixel
      for (int sy = 0; sy < numSamples; sy++) {
        for (int sx = 0; sx < numSamples; sx++) {
          // Calculate the subpixel offset
          double xOffset = (sx + 0.5) / numSamples;
          double yOffset = (sy + 0.5) / numSamples;

          // Convert pixel (i, j) to normalized device coordinates (NDC)
          double x = (i + xOffset) / buffer_width;
          double y = (j + yOffset) / buffer_height;

          // Trace the ray for this subpixel
          color += trace(x, y);
        }
      }

      // Average the color over all samples
      color /= (numSamples * numSamples);

      // Set the averaged color to the pixel
      setPixel(i, j, color);
    }
  }

  return 1; // Return 1 if success
}

bool RayTracer::checkRender() {
  // YOUR CODE HERE
  // FIXME: Return true if tracing is done.
  //        This is a helper routine for GUI.
  //
  // TIPS: Introduce an array to track the status of each worker thread.
  //       This array is maintained by the worker threads.
  
  // Check if all threads have finished
  for (const auto &status : threadStatus) {
    if (!status) {
      return false; // At least one thread is still running
    }
  }
  return true; // All threads are done
}

void RayTracer::waitRender() {
  // YOUR CODE HERE
  // FIXME: Wait until the rendering process is done.
  //        This function is essential if you are using an asynchronous
  //        traceImage implementation.
  //
  // TIPS: Join all worker threads here.
  
  // Join all worker threads
  for (auto &t : workerThreads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Clear the workerThreads vector after joining
  workerThreads.clear();
}


glm::dvec3 RayTracer::getPixel(int i, int j) {
  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;
  return glm::dvec3((double)pixel[0] / 255.0, (double)pixel[1] / 255.0,
                    (double)pixel[2] / 255.0);
}

void RayTracer::setPixel(int i, int j, glm::dvec3 color) {
  unsigned char *pixel = buffer.data() + (i + j * buffer_width) * 3;

  pixel[0] = (int)(255.0 * color[0]);
  pixel[1] = (int)(255.0 * color[1]);
  pixel[2] = (int)(255.0 * color[2]);
}
