// this is a test case to test whether rabit can recover model when 
// facing an exception
#include <rabit.h>
#include <utils.h>
#include "./toolkit_util.h"

using namespace rabit;

// kmeans model
class Model : public rabit::utils::ISerializable {
 public:
  // matrix of centroids
  Matrix centroids;
  // load from stream
  virtual void Load(rabit::utils::IStream &fi) {
    fi.Read(&centroids.nrow, sizeof(centroids.nrow));
    fi.Read(&centroids.ncol, sizeof(centroids.ncol));
    fi.Read(&centroids.data);
  }
  /*! \brief save the model to the stream */
  virtual void Save(rabit::utils::IStream &fo) const {
    fo.Write(&centroids.nrow, sizeof(centroids.nrow));
    fo.Write(&centroids.ncol, sizeof(centroids.ncol));
    fo.Write(centroids.data);
  }
  virtual void InitModel(unsigned num_cluster, unsigned feat_dim) {
    centroids.Init(num_cluster, feat_dim);
  }
  // normalize L2 norm
  inline void Normalize(void) {
    for (size_t i = 0; i < centroids.nrow; ++i) {
      float *row = centroids[i];
      double wsum = 0.0;
      for (size_t j = 0; j < centroids.ncol; ++j) {
        wsum += row[j] * row[j];
      }
      wsum = sqrt(wsum);
      if (wsum < 1e-6) return;
      float winv = 1.0 / wsum;
      for (size_t j = 0; j < centroids.ncol; ++j) {
        row[j] *= winv;
      }
    }
  }
};
inline void InitCentroids(const SparseMat &data, Matrix *centroids) {
  int num_cluster = centroids->nrow; 
  for (int i = 0; i < num_cluster; ++i) {
    int index = Random(data.NumRow());
    SparseMat::Vector v = data[index];
    for (unsigned j = 0; j < v.length; ++j) {
      (*centroids)[i][v[j].findex] = v[j].fvalue;
    }
  }
  for (int i = 0; i < num_cluster; ++i) {
    int proc = Random(rabit::GetWorldSize());
    rabit::Broadcast((*centroids)[i], centroids->ncol * sizeof(float), proc);
  }
}

inline double Cos(const float *row,
                  const SparseMat::Vector &v) {
  double rdot = 0.0, rnorm = 0.0; 
  for (unsigned i = 0; i < v.length; ++i) {
    rdot += row[v[i].findex] * v[i].fvalue;
    rnorm += v[i].fvalue * v[i].fvalue;
  }
  return rdot  / sqrt(rnorm);
}
inline size_t GetCluster(const Matrix &centroids,
                         const SparseMat::Vector &v) {
  size_t imin = 0;
  double dmin = Cos(centroids[0], v);
  for (size_t k = 1; k < centroids.nrow; ++k) {
    double dist = Cos(centroids[k], v);
    if (dist < dmin) {
      dmin = dist; imin = k;
    }    
  }
  return imin;
}
                  
int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf("Usage: <data_dir> num_cluster max_iter\n");
    return 0;
  }
  srand(0);
  // load the data 
  SparseMat data;
  data.Load(argv[1]);
  // set the parameters
  int num_cluster = atoi(argv[2]);
  int max_iter = atoi(argv[3]);
  // intialize rabit engine
  rabit::Init(argc, argv);
  // load model
  Model model; 
  int iter = rabit::LoadCheckPoint(&model);
  if (iter == 0) {
    rabit::Allreduce<op::Max>(&data.feat_dim, sizeof(data.feat_dim));
    model.InitModel(num_cluster, data.feat_dim);
    InitCentroids(data, &model.centroids);
    model.Normalize();
    utils::LogPrintf("[%d] start at %s\n",
                     rabit::GetRank(), rabit::GetProcessorName().c_str());
  } else {
    utils::LogPrintf("[%d] restart iter=%d\n", rabit::GetRank(), iter);    
  }
  const unsigned num_feat = data.feat_dim;
  // matrix to store the result
  Matrix temp;
  for (int r = iter; r < max_iter; ++r) { 
    temp.Init(num_cluster, num_feat + 1, 0.0f);
    const size_t ndata = data.NumRow();
    for (size_t i = 0; i < ndata; ++i) {
      SparseMat::Vector v = data[i];
      size_t k = GetCluster(model.centroids, v);
      for (size_t j = 0; j < v.length; ++j) {
        temp[k][v[j].findex] += v[j].fvalue;
      }
      temp[k][num_feat] += 1.0f;
    }
    rabit::Allreduce<op::Sum>(&temp.data[0], temp.data.size());
    for (int k = 0; k < num_cluster; ++k) {
      float cnt = temp[k][num_feat];
      for (unsigned i = 0; i < num_feat; ++i) {
        model.centroids[k][i] = temp[k][i] / cnt;
      }
    }
    model.Normalize();
    rabit::CheckPoint(model);
  }
  rabit::Finalize();
  return 0;
}
