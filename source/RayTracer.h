#ifndef RayTracer_h
#define RayTracer_h

#include <G3D/G3DAll.h>

/** 
    A non-recursive ray tracer
 */
class RayTracer : public ReferenceCountedObject {
 public:

    /** Convenience class for storing the settings associated
        with a ray tracer */
    class Settings {
    public:
        int                  width;
        int                  height;
        bool                 multithreaded;
        bool                 useTree;
        bool                 useShadows;
        int                  traceDepth;
        int                  raysPerPixel;
        bool                 pathTrace;
        Settings();
    };

    /** Class for storing the statistics associated with a 
        given ray tracing */
    class Stats {
    public:
        int                 lights;

        int                 triangles;

        /** width x height */
        int                 pixels;

        float               buildTriTreeTimeMilliseconds;
        float               rayTraceTimeMilliseconds;

        int                 primaryRays;
        int                 impulseRays;
        int                 shadowRays;
        int                 totalRays;
        Stats();
    };

 protected:
    
    /** Array of random number generators so that each threadID may
        have its own without using locks. */
    Array< shared_ptr<Random> > m_rnd;

    // The following are only valid during a call to render()
    shared_ptr<Image>        m_image;
    shared_ptr<LightingEnvironment> m_lighting;
    shared_ptr<Camera>       m_camera;
    Settings                 m_settings;
    Stats                    m_stats;
    TriTree                  m_triTree;
    Color3                   ambient;
    RayTracer();

    /** Called from GThread::runConcurrently2D(), which is invoked
        in traceAllPixels() */
    void traceOnePixel(int x, int y, int threadID);
    
    /** Called from render(). Writes to m_image. */
    void traceAllPixels(int numThreads);

    /**
       \param ray The ray in world space
       \param maxDistance Don't trace farther than this
       \param anyHit If true, return any surface hit, even if it is
       not hte first
       \return The surfel hit, or NULL if none was hit
    */
    shared_ptr<Surfel> castRay
        (const Ray&               ray,
         float                    maxDistance = finf(),
         bool                     anyHit = false) const;

    /** Computes L_o with emitted and direct scattering terms */
    Radiance3 L_o
         (const shared_ptr<Surfel>& surfel,
         const Vector3&            wo,
         Random&                   rnd,
         int                       bounces = 0);
    /**
       Computes direct illumination under point lights for Lambertian materials according to
       \f[
       \sum\limits_{j=0}^{N-1} [ B_{j} \mid \hat{w_{i}} \cdot \hat{n} \mid f_{X,\hat{n}}(\hat{w_{i}}, \hat{w_{o}}) ]
       \f]

       We can reasonably approximate the light at this surfel by summing the direct light from all light sources. 
       
       The biradiance is computed as 
       \f[
         B_{j} = \frac{\Phi}{4 \pi r^2}
       \f]
       Where Phi is the bulbpower in watts of a given light, and r is the distance in meters from the light (Y)
       to the surfel (X). 

       We approximate the material model for the scattering function using UniversalSurfel's lambertianReflectivity.
    */
    Radiance3 L_scatteredDirect
        (const shared_ptr<Surfel>& surfel,
         const Vector3&            wo,
         Random&                   rnd);

    /** Checks whether p1 is visible from p0 */
    float visible(const Vector3 p0, const Vector3& p1);

 public:
    
    static shared_ptr<RayTracer> create();
    
    /** Render the specified image using settings, lighting, and camera, and store 
        ray tracing data in stats */
    shared_ptr<Image> render
        (const Settings&                        settings,
         const Array< shared_ptr<Surface> >&    surfaceArray,
         const shared_ptr<LightingEnvironment>& lighting,
         const shared_ptr<Camera>&              camera,
         Stats&                                 stats);
    
};

#endif
