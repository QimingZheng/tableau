#pragma once

#include <assert.h>
#include <omp.h>
#include <pthread.h>

#include <algorithm>
#include <cstring>

typedef int64_t tableau_index_t;
typedef int64_t tableau_size_t;

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
    size_ = 0;
    while (capacity_ < size) {
      capacity_ *= 2;
    }
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
    capacity_ = Size() + other->Size();
    tableau_index_t* merged_index = new tableau_index_t[capacity_];
    T* merged_data = new T[capacity_];
    tableau_index_t left_index = 0, right_index = 0, next_index = 0;

    while (left_index < Size() && right_index < other->Size()) {
      if (index_[left_index] == other->index_[right_index]) {
        merged_data[next_index] = data_[left_index] + other->data_[right_index];
        merged_index[next_index] = index_[left_index];
        left_index++;
        right_index++;
      } else if (index_[left_index] < other->index_[right_index]) {
        merged_data[next_index] = data_[left_index];
        merged_index[next_index] = index_[left_index];
        left_index++;
      } else {
        merged_data[next_index] = other->data_[right_index];
        merged_index[next_index] = other->index_[right_index];
        right_index++;
      }
      next_index++;
    }
    while (left_index < Size()) {
      merged_data[next_index] = data_[left_index];
      merged_index[next_index] = index_[left_index];
      next_index++;
      left_index++;
    }
    while (right_index < other->Size()) {
      merged_data[next_index] = other->data_[right_index];
      merged_index[next_index] = other->index_[right_index];
      next_index++;
      right_index++;
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

  tableau_size_t Size() const { return size_; }

  void Append(tableau_index_t index, T value) {
    if (size_ >= capacity_) {
      capacity_ *= 2;
      T* new_data = new T[capacity_];
      tableau_index_t* new_index = new tableau_index_t[capacity_];

      std::memcpy(new_index, index_, sizeof(tableau_index_t) * capacity_);
      std::memcpy(new_data, data_, sizeof(T) * capacity_);

      delete index_;
      delete data_;
      index_ = new_index;
      data_ = new_data;
    }
    index_[size_] = index;
    data_[size_] = value;
    size_ += 1;
  }

  friend class Iterator;

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

  template <typename U>
  friend class List;

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
    List<T>* col = new List<T>(other);
    tableau_index_t index = other->index_[i];
    T scale = other->data_[i];
    col->Scale(scale);
    tableau->SetCol(index, col);
  }
  return tableau;
}
