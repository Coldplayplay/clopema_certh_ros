#include <certh_libs/Geometry.h>
#include <Eigen/Eigenvalues>

using namespace std ;

namespace certh_libs {

bool pointInsideCone(const Vec3 &x, const Vec3 &apex, const Vec3 &base, float aperture)
{
    // This is for our convenience
    float halfAperture = aperture/2.f;

    // Vector pointing to X point from apex
    Vec3 apexToXVect = apex - x ;
    apexToXVect.normalize() ;

    // Vector pointing from apex to circle-center point.
    Vec3 axisVect = apex - base;
    axisVect.normalize() ;

    // determine angle between apexToXVect and axis.

    double d = apexToXVect.dot(axisVect) ;

    bool isInInfiniteCone = fabs(d) > cos(halfAperture) ;

    if(!isInInfiniteCone) return false;

    // X is contained in cone only if projection of apexToXVect to axis
    // is shorter than axis.
    // We'll use dotProd() to figure projection length.

    bool isUnderRoundCap = apexToXVect.dot(axisVect)  < 1 ;

    return isUnderRoundCap;
}

double robustPlane3DFit(vector<Vec3> &x, Vec3  &c, Vec3 &u)
{
    int i, k, N = x.size() ;
    Vec3 u1, u2, u3 ;
    const int NITER = 1 ;

    double *weight = new double [N] ;
    double *res = new double [N] ;

    for( i=0 ; i<N ; i++ ) weight[i] = 1.0 ;

    double wsum = N ;
    double sigma ;

    for( k=0 ; k<NITER ; k++ )
    {

        c = Vec3::Zero() ;
        Matrix3 cov = Matrix3::Zero();

        for( i=0 ; i<N ; i++ )
        {
            const Vec3 &P = x[i] ;
            double w = weight[i] ;

            c += w * P ;
        }

        c /= wsum ;

        for( i=0 ; i<N ; i++ )
        {
            const Vec3 &P = x[i] ;
            double w = weight[i] ;

            cov += w *  (P - c) * (P - c).adjoint();

        }

        cov *= 1.0/wsum ;

        Matrix3 U ;
        Vec3 L ;

        Eigen::SelfAdjointEigenSolver<Matrix3> eigensolver(cov);
        L = eigensolver.eigenvalues() ;
        U = eigensolver.eigenvectors() ;

        // Recompute weights

        u1 = U.col(0) ;
        u2 = U.col(1) ;
        u3 = U.col(2) ;

        for( i=0 ; i<N ; i++ )
        {
            const Vec3 &P = x[i] ;

            double r = fabs((P-c).dot(u1));

            weight[i] = r ;
            res[i] = r ;
        }

        // Estimate residual variance


        sort(weight, weight + N) ;

        sigma = weight[N/2]/0.6745 ;

        // Update weights using Hubers scheme

        wsum = 0.0 ;

        for( i=0 ; i<N ; i++ )
        {

            double r = fabs(res[i]) ;

            if ( r <= sigma )  weight[i] = 1.0 ;
            else if ( r > sigma && r <= 3.0*sigma ) weight[i] = sigma/r ;
            else weight[i] = 0.0 ;

            wsum += weight[i] ;
        }
    }

    u = u1 ;

    delete weight ;
    delete res ;

    return sigma ;
}

Vec3 computeNormal(const PointCloud &pc, int x, int y)
{
    const int nrmMaskSize = 6 ;
    int w = pc.width, h = pc.height ;

    vector<Vec3> pts ;

    for(int i = y - nrmMaskSize ; i<= y + nrmMaskSize ; i++  )
        for(int j = x - nrmMaskSize ; j<= x + nrmMaskSize ; j++  )
        {
            if ( i < 0 || j < 0 || i > h-1 || j > w-1 ) continue ;

            PointT val = pc.at(j, i) ;

            if ( !pcl_isfinite(val.z) ) continue ;

            pts.push_back(Vec3(val.x, val.y, val.z)) ;
        }

    if ( pts.size() < 3 ) return Vec3() ;

    Vec3 u, c ;
    robustPlane3DFit(pts, c, u);

    if ( u(2) > 0 ) u = -u ;

    return u ;

}







}
