#include "RayCastAxisAligned3D.h"
#include "obcore/base/Logger.h"
#include "obcore/base/System.h"

#include <string.h>

namespace obvious {

RayCastAxisAligned3D::RayCastAxisAligned3D() {

}

RayCastAxisAligned3D::~RayCastAxisAligned3D() {

}

void RayCastAxisAligned3D::calcCoords(TsdSpace* space, double* coords, double* normals, unsigned char* rgb, unsigned int* cnt)
{
  Timer t;

  unsigned int partitionSize = space->getPartitionSize();
  unsigned int partitionsInX = space->getXDimension() / partitionSize;
  unsigned int partitionsInY = space->getYDimension() / partitionSize;
  unsigned int partitionsInZ = space->getZDimension() / partitionSize;
  double cellSize = space->getVoxelSize();

  *cnt = 0;

  TsdSpacePartition**** partitions = space->getPartitions();

  // buffer for registration of zero crossings: in each cell, only one zero crossing should be detected
  bool*** zeroCrossing;
  System<bool>::allocate(partitionSize, partitionSize, partitionSize, zeroCrossing);

#pragma omp parallel for
  // Leave out outmost partitions, since the triangulation of normals needs access to neighboring partitions
  for(unsigned int z=1; z<partitionsInZ-1; z++)
  {
    for(unsigned int y=1; y<partitionsInY-1; y++)
    {
      for(unsigned int x=1; x<partitionsInX-1; x++)
      {
        TsdSpacePartition* p = partitions[z][y][x];
        if(p->isInitialized())
        {
          if(!(p->isEmpty()))
          {
            // Traverse in x-direction
            TsdSpacePartition* p_prev = partitions[z][y][x-1];
            for(unsigned int pz=0; pz<p->getDepth(); pz++)
            {
              memset(zeroCrossing[pz][0], 0, (partitionSize)*(partitionSize)*sizeof(bool));
              for(unsigned int py=0; py<p->getHeight(); py++)
              {
                double tsd_prev = NAN;
                if(p_prev->isInitialized() && !(p_prev->isEmpty())) tsd_prev = (*p_prev)(pz, py, p_prev->getWidth()-1);
                double interp = 0.0;
                for(unsigned int px=0; px<p->getWidth(); px++)
                {
                  double tsd = (*p)(pz, py, px);
                  // Check sign change
                  if(tsd_prev * tsd < 0)
                  {
                    interp = tsd_prev / (tsd_prev - tsd);
#pragma omp critical
{
                    coords[*cnt]     = px*cellSize + (x * p->getWidth()) * cellSize + cellSize * (interp-1.0) ;
                    coords[(*cnt)+1] = py*cellSize + (y * p->getHeight()) * cellSize;
                    coords[(*cnt)+2] = pz*cellSize + (z * p->getDepth()) * cellSize;
                    if(normals)
                      space->interpolateNormal(&coords[*cnt], &(normals[*cnt]));
                    if(rgb)
                      space->interpolateTrilinearRGB(&coords[*cnt], &(rgb[*cnt]));
                    (*cnt)+=3;
                    zeroCrossing[pz][py][px] = true;
}
                  }
                  tsd_prev = tsd;
                }
              }
            }

            // Traverse in y-direction
            p_prev = partitions[z][y-1][x];
            for(unsigned int pz=0; pz<p->getDepth(); pz++)
            {
              for(unsigned int px=0; px<p->getWidth(); px++)
              {
                double tsd_prev = NAN;
                if(p_prev->isInitialized() && !(p_prev->isEmpty())) tsd_prev = (*p_prev)(pz, p_prev->getHeight()-1, px);
                double interp = 0.0;
                for(unsigned int py=0; py<p->getHeight(); py++)
                {
                  double tsd = (*p)(pz, py, px);
                  // Check sign change
                  if((!zeroCrossing[pz][py][px]) && (tsd_prev * tsd < 0))
                  {
                    interp = tsd_prev / (tsd_prev - tsd);
#pragma omp critical
{
                    coords[*cnt]     = px*cellSize + (x * p->getWidth()) * cellSize;
                    coords[(*cnt)+1] = py*cellSize + (y * p->getHeight()) * cellSize + cellSize * (interp-1.0);
                    coords[(*cnt)+2] = pz*cellSize + (z * p->getDepth()) * cellSize;
                    if(normals)
                      space->interpolateNormal(&coords[*cnt], &(normals[*cnt]));
                    if(rgb)
                      space->interpolateTrilinearRGB(&coords[*cnt], &(rgb[*cnt]));
                    (*cnt)+=3;
                    zeroCrossing[pz][py][px] = true;
}
                  }
                  tsd_prev = tsd;
                }
              }
            }

            // Traverse in z-direction
            p_prev = partitions[z-1][y][x];
            for(unsigned int px=0; px<p->getWidth(); px++)
            {
              for(unsigned int py=0; py<p->getHeight(); py++)
              {
                double tsd_prev = NAN;
                if(p_prev->isInitialized() && !(p_prev->isEmpty())) tsd_prev = (*p_prev)(p->getDepth()-1, py, px);
                double interp = 0.0;
                for(unsigned int pz=0; pz<p->getDepth(); pz++)
                {
                  double tsd = (*p)(pz, py, px);
                  // Check sign change
                  if((!zeroCrossing[pz][py][px]) && (tsd_prev * tsd < 0))
                  {
                    interp = tsd_prev / (tsd_prev - tsd);
#pragma omp critical
{
                    coords[*cnt]     = px*cellSize + (x * p->getWidth()) * cellSize;
                    coords[(*cnt)+1] = py*cellSize + (y * p->getHeight()) * cellSize;
                    coords[(*cnt)+2] = pz*cellSize + (z * p->getDepth()) * cellSize + cellSize * (interp-1.0);
                    if(normals)
                      space->interpolateNormal(&coords[*cnt], &(normals[*cnt]));
                    if(rgb)
                      space->interpolateTrilinearRGB(&coords[*cnt], &(rgb[*cnt]));
                    (*cnt)+=3;
}
                  }
                  tsd_prev = tsd;
                }
              }
            }
          }
        }
      }
    }
  }

  System<bool>::deallocate(zeroCrossing);

  LOGMSG(DBG_DEBUG, "Elapsed TSDF projection: " << t.getTime() << "ms");
  LOGMSG(DBG_DEBUG, "Raycasting finished! Found " << *cnt << " coordinates");
}

void RayCastAxisAligned3D::calcCoordsRoughly(TsdSpace* space, double* coords, double* normals, unsigned int* cnt)
{
  Timer t;

  unsigned int partitionsInX = space->getXDimension() / space->getPartitionSize();
  unsigned int partitionsInY = space->getYDimension() / space->getPartitionSize();
  unsigned int partitionsInZ = space->getZDimension() / space->getPartitionSize();
  double cellSize = space->getVoxelSize();

  *cnt = 0;

  TsdSpacePartition**** partitions = space->getPartitions();
  Matrix* C = TsdSpacePartition::getCellCoordsHom();

  double thresh = cellSize / space->getMaxTruncation();

#pragma omp parallel for
  for(unsigned int z=1; z<partitionsInZ-1; z++)
  {
    for(unsigned int y=1; y<partitionsInY-1; y++)
    {
      for(unsigned int x=1; x<partitionsInX-1; x++)
      {
        TsdSpacePartition* p = partitions[z][y][x];
        if(p->isInitialized())
        {
          double offset[3];
          p->getCellCoordsOffset(offset);

          unsigned int i = 0;
          for(unsigned int pz=0; pz<p->getDepth(); pz++)
          {
            for(unsigned int py=0; py<p->getHeight(); py++)
            {
              for(unsigned int px=0; px<p->getWidth(); px++, i++)
              {
                double tsd = (*p)(pz, py, px);
                // Check sign change
                if(tsd < thresh && tsd>0)
                {
#pragma omp critical
{
                  coords[*cnt]     = (*C)(i, 0) + offset[0];
                  coords[(*cnt)+1] = (*C)(i, 1) + offset[1];
                  coords[(*cnt)+2] = (*C)(i, 2) + offset[2];
                  if(normals)
                    space->interpolateNormal(&coords[*cnt], &(normals[*cnt]));
                  (*cnt)+=3;
}
                }
              }
            }
          }
        }
      }
    }
  }
  LOGMSG(DBG_DEBUG, "Elapsed TSDF projection: " << t.getTime() << "ms");
  LOGMSG(DBG_DEBUG, "Raycasting finished! Found " << *cnt << " coordinates");
}

}
