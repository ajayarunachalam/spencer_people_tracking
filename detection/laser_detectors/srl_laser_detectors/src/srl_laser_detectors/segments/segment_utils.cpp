#include <srl_laser_detectors/segments/segment_utils.h>

#include <limits>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

using namespace std;


namespace srl_laser_detectors {

void SegmentUtils::extractSegments(const sensor_msgs::LaserScan::ConstPtr& laserscan, const srl_laser_segmentation::LaserscanSegmentation::ConstPtr& segmentation, Segments& segments)
{
    //
    // Convert points of laserscan into Cartesian coordinates
    //
    const size_t numPoints = laserscan->ranges.size();

    vector<Point2D> pointsInCartesianCoords; pointsInCartesianCoords.reserve(numPoints);
    vector<bool> isPointValid; isPointValid.reserve(numPoints);

    for(size_t pointIndex = 0; pointIndex < laserscan->ranges.size(); pointIndex++) {
        double phi = laserscan->angle_min + laserscan->angle_increment * pointIndex + M_PI_2;
        double rho = laserscan->ranges[pointIndex];

        Point2D point;
        if(rho > laserscan->range_max || rho < laserscan->range_min) {
            // Out-of-range measurement, set x and y to NaN
            // NOTE: We cannot omit the measurement completely since this would screw up point indices
            point(0) = point(1) = numeric_limits<double>::quiet_NaN();
            isPointValid.push_back(false);
        }
        else {
            point(0) =  sin(phi) * rho;
            point(1) = -cos(phi) * rho;
            isPointValid.push_back(true);
        }

        pointsInCartesianCoords.push_back(point);
    }    


    //
    // Build segments by collecting Cartesian coordinates of all points per segment
    //
    segments.clear();
    segments.reserve(segmentation->segments.size());

    foreach(const srl_laser_segmentation::LaserscanSegment& inputSegment, segmentation->segments) {
        size_t numPointsInSegment = inputSegment.measurement_indices.size();
        Segment segment;
        segment.points.reserve(numPointsInSegment);
        segment.ranges.reserve(numPointsInSegment);
        segment.indices.reserve(numPointsInSegment);

        // These are used to calculate the median
        Point2D pointSum = Point2D::Zero();
        vector<double> xs, ys;
        xs.reserve(numPointsInSegment);
        ys.reserve(numPointsInSegment);

        size_t numValidInSegment = 0;
        foreach(unsigned int pointIndex, inputSegment.measurement_indices) {
            if(isPointValid[pointIndex]) { // include only valid measurements in segment
                const Point2D& point = pointsInCartesianCoords[pointIndex];
                segment.points.push_back( point ); 
                segment.ranges.push_back( laserscan->ranges[pointIndex] );
                segment.indices.push_back( pointIndex );

                // for calculating mean, median
                pointSum += point;
                xs.push_back(point(0));
                ys.push_back(point(1));

                numValidInSegment++;
            }
        }

        // Skip segments without valid points
        if(0 == numValidInSegment) continue;

        // Calculate mean, median of segment
        segment.mean = pointSum / (double) numValidInSegment;
        sort(xs.begin(), xs.end());
        sort(ys.begin(), ys.end());

        if(numValidInSegment & 1) {
            segment.median(0) = xs[ (numValidInSegment-1) / 2 ];
            segment.median(1) = ys[ (numValidInSegment-1) / 2 ];
        }
        else {
            segment.median(0) = ( xs[numValidInSegment/2-1] + xs[numValidInSegment/2] ) / 2.0;
            segment.median(1) = ( ys[numValidInSegment/2-1] + ys[numValidInSegment/2] ) / 2.0;
        }

        // Set points that directly precede and succeed the segment
        size_t firstIndex = inputSegment.measurement_indices.front();
        if(firstIndex == 0 || !isPointValid[firstIndex-1]) {
            segment.precedingPoint.setConstant(numeric_limits<double>::quiet_NaN());
        }
        else {
            segment.precedingPoint = pointsInCartesianCoords[firstIndex-1];
        }

        size_t lastIndex = inputSegment.measurement_indices.back();
        if(lastIndex >= numPoints -1 || !isPointValid[lastIndex+1]) {
            segment.succeedingPoint.setConstant(numeric_limits<double>::quiet_NaN());
        }
        else {
            segment.succeedingPoint = pointsInCartesianCoords[lastIndex+1];
        }

        // Store the segment
        segments.push_back(segment);
    }        
}


} // end of namespace srl_laser_detectors