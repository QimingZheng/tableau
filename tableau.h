#pragma once

#include <assert.h>
#include <omp.h>
#include <pthread.h>

#include <algorithm>
#include <cstring>

typedef int64_t tableau_index_t;
typedef int64_t tableau_size_t;

template <typename T>
class SparseTableau;

template <typename T>
class Tableau;

/**
 * A list is the abstraction of a row or column in a simplex tableau.
 */
template <typename T>
class List {
 public:
  class Iterator {
   public:
    Iterator(List<T>* list) : list_(list) { index_ = 0; }

    tableau_index_t Index() { return list_->index_[index_]; }
    T Data() { return list_->data_[index_]; }

    Iterator* Next() {
      index_ += 1;
      return this;
    }

    bool IsEnd() { return index_ >= list_->size_; }

   private:
    List<T>* list_;
    tableau_index_t index_;
  };

  Iterator* Begin() { return new Iterator(this); }

  List(tableau_size_t size = 0) {
    capacity_ = 1;
    while (capacity_ < size) {
      capacity_ <<= 1;
    }
    size_ = 0;
    index_ = new tableau_index_t[capacity_];
    data_ = new T[capacity_];
  }
  ~List() {
    if (capacity_ > 0) {
      delete data_;
      delete index_;
    }
    data_ = nullptr;
    index_ = nullptr;
    size_ = 0;
    capacity_ = 0;
  }

  List(const List<T>* other) {
    size_ = other->size_;
    capacity_ = other->capacity_;
    index_ = new tableau_index_t[capacity_];
    data_ = new T[capacity_];
    std::memcpy(index_, other->index_, sizeof(tableau_index_t) * capacity_);
    std::memcpy(data_, other->data_, sizeof(T) * capacity_);
  }

  void Clear() { size_ = 0; }

  T At(tableau_index_t index) {
    tableau_index_t pos = BinarySearch(index);
    if (pos >= 0) return data_[pos];
    return 0;
  }

  /* Set the element at index, if no element is at index, skip. */
  void Set(tableau_index_t index, T value) {
    tableau_index_t pos = BinarySearch(index);
    if (pos >= 0) data_[pos] = value;
  }

  void Add(const List<T>* other) {
    if (other->Size() == 0) return;
    capacity_ = Size() + other->Size();
    tableau_index_t* merged_index = new tableau_index_t[capacity_];
    T* merged_data = new T[capacity_];
    tableau_index_t left_index = 0, right_index = 0, next_index = 0;

    while (left_index < Size() && right_index < other->Size()) {
      if (index_[left_index] == other->index_[right_index]) {
        T sum = data_[left_index] + other->data_[right_index];
        if (sum != 0) {
          merged_data[next_index] =
              data_[left_index] + other->data_[right_index];
          merged_index[next_index] = index_[left_index];
          next_index++;
        }
        left_index++;
        right_index++;
      } else if (index_[left_index] < other->index_[right_index]) {
        if (data_[left_index] != 0) {
          merged_data[next_index] = data_[left_index];
          merged_index[next_index] = index_[left_index];
          next_index++;
        }
        left_index++;
      } else {
        if (other->data_[right_index] != 0) {
          merged_data[next_index] = other->data_[right_index];
          merged_index[next_index] = other->index_[right_index];
          next_index++;
        }
        right_index++;
      }
    }
    while (left_index < Size()) {
      if (data_[left_index] != 0) {
        merged_data[next_index] = data_[left_index];
        merged_index[next_index] = index_[left_index];
        next_index++;
      }
      left_index++;
    }
    while (right_index < other->Size()) {
      if (other->data_[right_index] != 0) {
        merged_data[next_index] = other->data_[right_index];
        merged_index[next_index] = other->index_[right_index];
        next_index++;
      }
      right_index++;
    }
    size_ = next_index;
    delete index_;
    delete data_;
    index_ = merged_index;
    data_ = merged_data;
  }

  void Mul(const List<T>* other) {
    capacity_ = std::min(Size(), other->Size());
    tableau_index_t* merged_index = new tableau_index_t[capacity_];
    T* merged_data = new T[capacity_];
    tableau_index_t left_index = 0, right_index = 0, next_index = 0;

    while (left_index < Size() && right_index < other->Size()) {
      if (index_[left_index] == other->index_[right_index]) {
        T prod = data_[left_index] * other->data_[right_index];
        if (prod != 0) {
          merged_data[next_index] =
              data_[left_index] * other->data_[right_index];
          merged_index[next_index] = index_[left_index];
          next_index++;
        }
        left_index++;
        right_index++;
      } else if (index_[left_index] < other->index_[right_index]) {
        left_index++;
      } else {
        right_index++;
      }
    }
    size_ = next_index;
    delete index_;
    delete data_;
    index_ = merged_index;
    data_ = merged_data;
  }

  void Scale(const T scale) {
    for (tableau_index_t i = 0; i < size_; i++) data_[i] *= scale;
  }

  T Dot(const List<T>* other) const {
    tableau_index_t left = 0, right = 0;
    T product = 0;
    while (left < Size() and right < other->Size()) {
      if (index_[left] == other->index_[right]) {
        product += data_[left] * other->data_[right];
        left += 1;
        right += 1;
      } else if (index_[left] < other->index_[right])
        left += 1;
      else
        right += 1;
    }
    return product;
  }

  template <typename R>
  List<R>* Map(R (*transform)(const T&)) const {
    List<R>* list = new List<R>(Size());
#pragma omp parallel for
    for (tableau_index_t i = 0; i < Size(); i++) {
      list->index_[i] = index_[i];
      list->data_[i] = transform(data_[i]);
    }
    list->size_ = Size();
    return list;
  }

  typedef std::pair<tableau_index_t, T> ReduceStruct;
  static ReduceStruct MinReduce(const ReduceStruct& a, const ReduceStruct& b) {
    return (b.second < a.second) ? b : a;
  }

  ReduceStruct Reduce(ReduceStruct (*reduce)(const ReduceStruct&,
                                             const ReduceStruct&),
                      ReduceStruct initial_value) const {
    for (tableau_index_t i = 0; i < Size(); i++) {
      initial_value = reduce({index_[i], data_[i]}, initial_value);
    }
    return initial_value;
  }

  Tableau<T>* Cross(const List<T>* other, tableau_size_t rows,
                    tableau_size_t cols) const;

  SparseTableau<T>* SparseCross(const List<T>* other) const;

  tableau_size_t Size() const { return size_; }

  void Append(tableau_index_t index, T value) {
    if (size_ >= capacity_) {
      capacity_ *= 2;
      T* new_data = new T[capacity_];
      tableau_index_t* new_index = new tableau_index_t[capacity_];

      std::memcpy(new_index, index_, sizeof(tableau_index_t) * size_);
      std::memcpy(new_data, data_, sizeof(T) * size_);

      delete index_;
      delete data_;
      index_ = new_index;
      data_ = new_data;
    }
    index_[size_] = index;
    data_[size_] = value;
    size_ += 1;
  }
  void Pop(tableau_index_t last_index = -1) {
    if (size_ > 0) {
      if (last_index > 0) {
        if (index_[size_ - 1] != last_index) return;
      }
      size_ -= 1;
    }
  }

  friend class Iterator;

  template <typename U>
  friend class SparseTableau;

 private:
  tableau_size_t size_ = 0;
  tableau_size_t capacity_ = 0;
  tableau_index_t* index_ = nullptr;
  T* data_ = nullptr;

  tableau_index_t BinarySearch(tableau_index_t index) {
    tableau_index_t lower = 0, upper = size_ - 1;
    while (lower <= upper) {
      tableau_index_t middle = (lower + upper) / 2;
      if (index_[middle] == index) {
        return middle;
      }
      if (index_[middle] > index) {
        upper = middle - 1;
      } else {
        lower = middle + 1;
      }
    }
    return -1;
  }
};

/* A Simplex Tableau. */
template <typename T>
class Tableau {
 public:
  Tableau(tableau_size_t rows, tableau_size_t columns)
      : rows_(rows), columns_(columns) {
    row_heads_ = new List<T>*[rows];
#pragma omp parallel for
    for (tableau_size_t i = 0; i < rows; i++) row_heads_[i] = new List<T>();
    col_heads_ = new List<T>*[columns];
#pragma omp parallel for
    for (tableau_size_t i = 0; i < columns; i++) col_heads_[i] = new List<T>();
  }
  ~Tableau() {
    for (auto i = 0; i < rows_; i++) {
      delete row_heads_[i];
    }
    for (auto i = 0; i < columns_; i++) {
      delete col_heads_[i];
    }
    delete row_heads_;
    delete col_heads_;
  }

  T At(tableau_index_t row, tableau_index_t col) { return Row(row)->At(col); }

  List<T>* Row(tableau_index_t row) const { return row_heads_[row]; }
  List<T>* Col(tableau_index_t col) const { return col_heads_[col]; }

  void Add(const Tableau<T>* other) {
    assert(rows_ == other->rows_);
    assert(columns_ == other->columns_);

#pragma omp parallel for
    for (tableau_index_t row = 0; row < rows_; row++)
      Row(row)->Add(other->Row(row));

#pragma omp parallel for
    for (tableau_index_t col = 0; col < columns_; col++)
      Col(col)->Add(other->Col(col));
  }

  void Add(const SparseTableau<T>* other) {
#pragma omp parallel for
    for (tableau_index_t row = 0; row < other->Rows(); row++)
      Row(other->SparseRowIndexOf(row))->Add(other->Row(row));

#pragma omp parallel for
    for (tableau_index_t col = 0; col < other->Cols(); col++)
      Col(other->SparseColIndexOf(col))->Add(other->Col(col));
  }

  template <typename U>
  friend class List;

  void AppendRow(tableau_index_t row, List<T>* list) {
    // TODO: delete row_heads_[row]
    row_heads_[row] = list;
    for (auto iter = list->Begin(); iter->IsEnd() == false;
         iter = iter->Next()) {
      Col(iter->Index())->Append(row, iter->Data());
    }
  }
  void AppendCol(tableau_index_t col, List<T>* list) {
    // TODO: delete col_heads_[col]
    col_heads_[col] = list;
    for (auto iter = list->Begin(); iter->IsEnd() == false;
         iter = iter->Next()) {
      Row(iter->Index())->Append(col, iter->Data());
    }
  }

  void AppendExtraCol(List<T>* list) {
    List<T>** new_col_heads = new List<T>*[columns_ + 1];
    for (auto i = 0; i < columns_; i++) new_col_heads[i] = col_heads_[i];
    new_col_heads[columns_] = list;
    delete col_heads_;
    col_heads_ = new_col_heads;
    columns_ += 1;
    for (auto iter = list->Begin(); iter->IsEnd() == false;
         iter = iter->Next()) {
      Row(iter->Index())->Append(columns_ - 1, iter->Data());
    }
  }
  void RemoveExtraCol() {
    auto last_col = col_heads_[columns_ - 1];
    columns_ -= 1;
    for (auto iter = last_col->Begin(); iter->IsEnd() == false;
         iter = iter->Next()) {
      Row(iter->Index())->Pop(columns_);
    }
  }

  tableau_size_t Rows() const { return rows_; }
  tableau_size_t Cols() const { return columns_; }

 private:
  void SetRow(tableau_index_t row, List<T>* list) {
    delete row_heads_[row];
    row_heads_[row] = list;
  }
  void SetCol(tableau_index_t col, List<T>* list) {
    delete col_heads_[col];
    col_heads_[col] = list;
  }
  tableau_size_t rows_, columns_;
  List<T>** row_heads_;
  List<T>** col_heads_;
};

template <typename T>
Tableau<T>* List<T>::Cross(const List<T>* other, tableau_size_t rows,
                           tableau_size_t cols) const {
  Tableau<T>* tableau = new Tableau<T>(rows, cols);
#pragma omp parallel for
  for (tableau_index_t i = 0; i < Size(); i++) {
    List<T>* row = new List<T>(other);
    tableau_index_t index = index_[i];
    T scale = data_[i];
    row->Scale(scale);
    tableau->SetRow(index, row);
  }
#pragma omp parallel for
  for (tableau_index_t i = 0; i < other->Size(); i++) {
    List<T>* col = new List<T>(this);
    tableau_index_t index = other->index_[i];
    T scale = other->data_[i];
    col->Scale(scale);
    tableau->SetCol(index, col);
  }
  return tableau;
}

template <typename T>
class SparseTableau {
 public:
  SparseTableau(tableau_size_t rows, tableau_size_t cols) {
    sparse_row_heads_ = new List<List<T>*>(rows);
    sparse_row_heads_->size_ = rows;
    sparse_col_heads_ = new List<List<T>*>(cols);
    sparse_col_heads_->size_ = cols;
  }
  ~SparseTableau() {
    if (sparse_row_heads_ != nullptr) {
      for (auto i = 0; i < sparse_row_heads_->Size(); i++) {
        delete sparse_row_heads_->data_[i];
      }
    }
    if (sparse_col_heads_ != nullptr) {
      for (auto i = 0; i < sparse_col_heads_->Size(); i++) {
        delete sparse_col_heads_->data_[i];
      }
    }
    delete sparse_row_heads_;
    delete sparse_col_heads_;
  }

  List<T>* Row(tableau_index_t row) const {
    return sparse_row_heads_->data_[row];
  }
  List<T>* Col(tableau_index_t col) const {
    return sparse_col_heads_->data_[col];
  }
  tableau_index_t SparseRowIndexOf(tableau_index_t row) const {
    return sparse_row_heads_->index_[row];
  }
  tableau_index_t SparseColIndexOf(tableau_index_t col) const {
    return sparse_col_heads_->index_[col];
  }

  tableau_size_t Rows() const { return sparse_row_heads_->Size(); }
  tableau_size_t Cols() const { return sparse_col_heads_->Size(); }

  template <typename U>
  friend class List;

 private:
  void SetRow(tableau_index_t row, tableau_index_t sparse_row_index,
              List<T>* list) {
    sparse_row_heads_->index_[row] = sparse_row_index;
    sparse_row_heads_->data_[row] = list;
  }
  void SetCol(tableau_index_t col, tableau_index_t sparse_col_index,
              List<T>* list) {
    sparse_col_heads_->index_[col] = sparse_col_index;
    sparse_col_heads_->data_[col] = list;
  }
  List<List<T>*>* sparse_row_heads_ = nullptr;
  List<List<T>*>* sparse_col_heads_ = nullptr;
};

template <typename T>
SparseTableau<T>* List<T>::SparseCross(const List<T>* other) const {
  SparseTableau<T>* sparse_tableau =
      new SparseTableau<T>(Size(), other->Size());
#pragma omp parallel for
  for (tableau_index_t i = 0; i < Size(); i++) {
    List<T>* row = new List<T>(other);
    tableau_index_t index = index_[i];
    T scale = data_[i];
    row->Scale(scale);
    sparse_tableau->SetRow(i, index, row);
  }
#pragma omp parallel for
  for (tableau_index_t i = 0; i < other->Size(); i++) {
    List<T>* col = new List<T>(this);
    tableau_index_t index = other->index_[i];
    T scale = other->data_[i];
    col->Scale(scale);
    sparse_tableau->SetCol(i, index, col);
  }
  return sparse_tableau;
}
