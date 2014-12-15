#include "RayTracer.h"

float EPSILON = 0.0001;
RayTracer::Settings::Settings() :
    width(160),
    height(90),
    multithreaded(true),
    useTree(true),  
    useShadows(true),
    traceDepth(1),
    raysPerPixel(1),
    pathTrace(false) {

    # ifdef G3D_DEBUG
        // If we’re debugging, we probably don’t want threads by default
        multithreaded = false;
    # endif
}

RayTracer::Stats::Stats() :
    lights(0),
    triangles(0),
    pixels(0),
    buildTriTreeTimeMilliseconds(0),
    rayTraceTimeMilliseconds(0),
    primaryRays(0),
    impulseRays(0),
    shadowRays(0),
    totalRays(0) {}

RayTracer::RayTracer() {
    m_rnd.resize(System::numCores());
    for (int i = 0; i < m_rnd.size(); ++i) {
        // Use a different seed for each and do not be threadsafe
        m_rnd[i] = shared_ptr<Random>(new Random(i, false));
    }
    ambient = Color3(0.05f);
}

shared_ptr<RayTracer> RayTracer::create() {
    return shared_ptr<RayTracer>(new RayTracer());
}

shared_ptr<Image> RayTracer::render
(const Settings& settings,
 const Array< shared_ptr<Surface> >& surfaceArray,
 const shared_ptr<LightingEnvironment>& lighting,
 const shared_ptr<Camera>& camera,
 Stats& stats) {
    RealTime start;
    debugAssert(notNull(lighting) && notNull(camera));
    m_stats = stats;
    m_settings = settings;
    m_lighting = lighting;
    m_camera = camera;

    m_stats.lights = m_lighting->lightArray.size();
    
    // Build the TriTree
    start = System::time();
    m_triTree.setContents(surfaceArray);
    m_stats.buildTriTreeTimeMilliseconds = float((System::time() - start) / units::milliseconds());
    // Allocate the image
    m_image = Image::create(settings.width, settings.height, ImageFormat::RGB32F());
    m_stats.pixels = settings.width * settings.height;
    // Render the image
    start = System::time();
    const int numThreads = settings.multithreaded ? GThread::NUM_CORES : 1;
    traceAllPixels(numThreads);

    //here are the rest of the stats
    m_stats.rayTraceTimeMilliseconds = float((System::time() - start) / units::milliseconds());
    m_stats.triangles = m_triTree.size();
    m_stats.lights = m_lighting->lightArray.size();

    shared_ptr<Image> temp(m_image);
    
    m_lighting.reset();
    m_stats.totalRays = m_stats.impulseRays + m_stats.primaryRays + m_stats.shadowRays;
    stats = m_stats;
    return temp;
}

void RayTracer::traceAllPixels(int numThreads) {
    GThread::runConcurrently2D(Point2int32(0,0), Point2int32(m_settings.width, m_settings.height), this, &RayTracer::traceOnePixel, numThreads);
}

void RayTracer::traceOnePixel(int x, int y, int threadID) {
    Rect2D viewport(Vector2(m_settings.width, m_settings.height));    
    int numRays = m_settings.raysPerPixel;
    m_stats.primaryRays += numRays;
    Radiance3 R(0.0f);
    if (numRays == 1) {
        Ray ray(m_camera->worldRay(x + 0.5f, y + 0.5f, viewport));
        shared_ptr<Surfel> surfaceHit( castRay(ray) );
        if (notNull(surfaceHit)) {
            // Negative direction since we need it is relative to the surfel
            R += L_o( surfaceHit, -ray.direction(), *m_rnd[threadID], m_settings.traceDepth);
        }
    } else {
        for (int i = 0; i < numRays; ++i) {
            Ray ray(m_camera->worldRay(x + m_rnd[threadID]->uniform(), y + m_rnd[threadID]->uniform(), viewport));
            shared_ptr<Surfel> surfaceHit( castRay(ray) );
            if (notNull(surfaceHit)) {
                R += (L_o(surfaceHit, -ray.direction(), *m_rnd[threadID], m_settings.traceDepth)) / float(numRays);
            }
        }
    }
    if (R == Radiance3(0.0f)) {
        R = ambient;
    }
    m_image->set(Vector2int32(x, y), R);
}
shared_ptr<Surfel> RayTracer::castRay
    (const Ray& ray,
     float maxDistance,
     bool anyHit) const {
    
    // Distance from P to X
    float distance(maxDistance);
    shared_ptr<Surfel> surfel;
    if (m_settings.useTree) {
        // Treat the triTree as a tree
        surfel = m_triTree.intersectRay(ray, distance, anyHit);
    } else {
        // Treat the triTree as an array
        Tri::Intersector intersector;
        for (int t = 0; t < m_triTree.size(); ++t) {
            const Tri& tri = m_triTree[t];
            intersector(ray, m_triTree.cpuVertexArray(), tri,
                        anyHit, distance);
        }
        surfel = intersector.surfel();
    }
    
    return surfel;
}

Radiance3 RayTracer::L_o(const shared_ptr<Surfel>& surfel, const Vector3& wo, Random& rnd, int bounces) {
    // direct illumination;
   if (isNull(surfel)) {
       return ambient;
   }
   Radiance3 radiance = surfel->emittedRadiance(wo) + L_scatteredDirect(surfel, wo, rnd);

   radiance += surfel->reflectivity(rnd) * ambient;
   if (bounces > 1) {
       Surfel::ImpulseArray impulseArray;

       surfel->getImpulses(PathDirection::EYE_TO_SOURCE, wo, impulseArray);
       if (m_settings.pathTrace) {
           Surfel::Impulse impulse;
           surfel->scatter(PathDirection::SOURCE_TO_EYE, wo, false, rnd, impulse.magnitude, impulse.direction);
           impulseArray.append(impulse);
       }
       for (int i = 0; i < impulseArray.size(); ++i) {

           const Surfel::Impulse& impulse = impulseArray[i];
           const Vector3& offset = surfel->geometricNormal * sign(impulse.direction.dot(surfel->geometricNormal)) * EPSILON;
           Ray nextRay(surfel->position + offset, impulse.direction);
           shared_ptr<Surfel> nextSurfel = castRay(nextRay);
           ++m_stats.impulseRays;
           radiance += L_o(nextSurfel, -nextRay.direction(), rnd, bounces - 1) * impulse.magnitude;
       }
   }

   return radiance;
}

float RayTracer::visible(const Vector3 p0, const Vector3& p1) {
    Ray ray = Ray::fromOriginAndDirection(p0, (p1 - p0).direction()).bumpedRay(EPSILON);
    shared_ptr<UniversalSurfel> surfel = dynamic_pointer_cast<UniversalSurfel>(castRay(ray, (p1 -p0).magnitude() - 2 * EPSILON, false));
    ++m_stats.shadowRays;
    return float(isNull(surfel));
    /*if (isNull(surfel)) {
        return 1.0f;
    } else {
        return (1 - surfel->coverage) * visible(surfel->position, p1);
    }*/
}

Radiance3 RayTracer::L_scatteredDirect(const shared_ptr<Surfel>& surfel, const Vector3& wo, 
                            Random& rnd) {
    
    Radiance3 L(0.0f); // Radiance3 is Watts/ m^2 sr
    Vector3 X = surfel->position;
    Vector3 n = surfel->shadingNormal;
    for (size_t j = 0; j < m_lighting->lightArray.size(); ++j) {
        const shared_ptr<Light> light = m_lighting->lightArray[(int) j];
        Vector3 Y = light->position().xyz(); 
        Vector3 wi = (Y - X).direction(); // unit vector
        
        float r = (Y - X).magnitude(); // radius is in meters. Distance from light to surface
        if (light->spotHalfAngle() < pif() / 2.0f) {
            if (!light->inFieldOfView(wi)) {
                continue;
            }
        }
        Power3  Phi  = light->bulbPower(); // Power3 is in Joules / second= Watts
        Biradiance3 B = Phi / (4 * pif() * square(r)); //Biradiance3 is in Watts / m^2
        if (wo.dot(n) > 0 && wi.dot(n) > 0) {
            shared_ptr<UniversalSurfel> u = dynamic_pointer_cast<UniversalSurfel>(surfel);
            L += (m_settings.useShadows ? visible(Y, X + surfel->geometricNormal * EPSILON) : 1.0f) * B * (u->lambertianReflectivity / pif()) * abs(wi.dot(n));
            // Radiance3 is Watts/ m^2 sr.  So, Biradiance * 1/sr
        }
    }
    
    return L;
}
