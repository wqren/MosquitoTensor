// Copyright Aaryn Tonita, 2011
// Distributed under the Gnu general public license
#include "Tensor.h"
#include <cstdlib>
#include <cassert>
#define DIMENSION 4

using namespace Mosquito;

Tensor::Tensor(const char* indexString, double* data)
 : deleteComponents(false) {
  // Determine rank.
  rank = -1;
  for (int i = 0; i < 33 && rank < 0; i++) { 
    if (indexString[i] == '\0') {
      rank = i/2;
    }
  }
  assert(rank >= 0);

  // Initialise.
  types = new IndexType[rank];
  labels = new char[rank];

  // Initialize components array if it is not given
  if(!data) {
    components = new double[ipow(DIMENSION, rank)];
    deleteComponents = true;
  } else {
    components = data;
  }

  for (int i = 0; i < ipow(DIMENSION, rank); i++) components[i] = 0;

  // Determine index type and label.
  for (int i = 0; i < rank; i++) {
    if (indexString[2*i] == '^') {
      types[i] = UP;
    } else if (indexString[2*i] == '_') {
      types[i] = DOWN;
    } else {
      assert(false);
    }
    labels[i] = indexString[2*i + 1];
  }
}

Tensor::Tensor(int Rank, ...) {
  if (Rank == 0) {
    init(Rank,NULL);
  } else {
    IndexType Types[Rank];
    va_list listPointer;
    va_start(listPointer, Rank);
    for (int i = 0; i < Rank; i++) {
      Types[i] = (IndexType)va_arg(listPointer, int);
    }
    va_end(listPointer);
    init(Rank, Types);
  }
}

Tensor::Tensor(int Rank, const IndexType* Types) {
  init(Rank,Types);
}

void Tensor::init(int Rank, const IndexType* Types) {
  rank = Rank;
  types = new IndexType[rank];
  labels = new char[rank];
  components = new double[ipow(DIMENSION,rank)];
  for (int i = 0; i < rank; i++) {
    types[i] = Types[i];
    labels[i] = i + 1; // Which is not good ascii, but we don't want to make a mess.
  }
  for (int i = 0; i < ipow(DIMENSION,rank); i++) components[i] = 0.0;
}

Tensor::Tensor(const Tensor &original) {
  rank = original.rank;
  types = new IndexType[rank];
  labels = new char[rank];
  components = new double[ipow(DIMENSION,rank)];
  for (int i = 0; i < rank; i++) {
    types[i] = original.types[i];
    labels[i] = original.labels[i];
  }
  for (int i = 0; i < ipow(DIMENSION,rank); i++) {
    components[i] = original.components[i];
  }
}

Tensor::Tensor(const IndexedTensor &original) {
  // Copy all the data.
  // todo
  rank = original.getRank();
  types = new IndexType[rank];
  labels = new char[rank];
  components = new double[ipow(DIMENSION,rank)];
  const IndexType* originalTypes = original.getTypes();
  const char* originalLabels = original.getLabels();
  for (int i = 0; i < rank; i++) {
    types[i] = originalTypes[i];
    labels[i] = originalLabels[i];
  }
  if (rank > 0) {
    int indices[rank];
    for (int i = 0; i < ipow(DIMENSION,rank); i++) {
      indexToIndices(i, indices);
      components[i] = original.computeComponent(indices);
    }
  } else {
    components[0] = original.computeComponent(0);
  }
}

Tensor::~Tensor() {
  delete[] types;
  delete[] labels;
  if(deleteComponents)
    delete[] components;
}

Tensor Tensor::contract(int index1, int index2) const {
  // Build the result type.
  assert(types[index1] != types[index2]);
  // Won't use the last two, but need to allocate more than 0...
  Tensor::IndexType resultTypes[rank];
  int runningIndex = 0;
  for (int i = 0; i < rank; i++) {
    if (i != index1 && i != index2) {
      resultTypes[runningIndex] = types[i];
    }
  }
  Tensor result(rank-2, resultTypes);

  int indices[rank];
  int resultIndices[result.getRank()];
  for (int i = 0; i < ipow(DIMENSION, result.getRank()); i++) {
    result.indexToIndices(i,resultIndices);
    runningIndex = 0;
    // The free indices...
    for (int j = 0; j < rank; j++) {
      if (j != index1 && j!= index2) {
        indices[j] = resultIndices[runningIndex];
        runningIndex++;
      }
    }

    // Sum over the contracting indices.
    double value = 0.0;
    for (int j = 0; j < DIMENSION; j++) {
      indices[index1] = j;
      indices[index2] = j;
      value += components[index(indices)];
    }
    result.components[i] = value;
  }
  return result;
}

Tensor & Tensor::operator*=(const double scalar) {
  for (int i = 0; i < ipow(DIMENSION, rank); i++) {
    components[i] *= scalar;
  }
  return *this;
}

Tensor Tensor::operator*(const double scalar) const {
  Tensor result = *this;
  result *= scalar;
  return result;
}

IndexedTensor Tensor::operator[](const char* names) {
  IndexedTensor indexed(rank, types, components, names);
  return indexed;
}

Tensor Tensor::operator*(const Tensor& tensor) const {
  // tensoruild the result type...
  IndexType resultTypes[rank + tensor.getRank()];
  char resultIndexes[rank + tensor.getRank()];
  const Tensor::IndexType* bTypes = tensor.getTypes();
  for (int i = 0; i < rank; i++) {
    resultTypes[i] = types[i];
  }
  for (int i = 0; i < tensor.getRank(); i++) {
    resultTypes[rank+i] = bTypes[i];
  }
  Tensor result(rank+tensor.getRank(), resultTypes, resultIndexes);

  int resultIndices[result.getRank()];
  int indices[rank];
  int tensorIndices[tensor.getRank()];
  for (int i = 0; i < ipow(DIMENSION, result.getRank()); i++) {
    result.indexToIndices(i, resultIndices);
    for (int j = 0; j < rank; j++) indices[j] = resultIndices[j];
    for (int j = 0; j < tensor.getRank(); j++) tensorIndices[j] = resultIndices[rank+j];
    double value = components[index(indices)]
      *tensor.components[tensor.index(tensorIndices)];
    result.components[i] = value;
  }
  return result;
}

Tensor & Tensor::operator=(const Tensor &tensor) {
  for (int i = 0; i < rank; i++) {
    assert(types[i] == tensor.types[i]);
  }

  // Get the labelling permutation.
  const char* labels2 = tensor.getLabels();
  int permute[rank];
  bool permutable = permutation(labels2, permute);
  assert(permutable);

  int indices[rank];
  int indices2[rank];
  for (int i = 0; i < ipow(DIMENSION, rank); i++) {
    indexToIndices(i, indices);
    for (int j = 0; j < rank; j++) indices2[permute[j]] = indices[j];
    components[i] = tensor(indices2);
  }
  return *this;
}

Tensor & Tensor::relabel(const char *newLabels) {
  for (int i = 0; i < rank; i++) {
    labels[i] = newLabels[i];
  }
  return *this;
}
