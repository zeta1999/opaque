#include "Crypto.h"
#include "NewInternalTypes.h"
#include "math.h"
#include "sgx_trts.h"
#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */

#include "Join.h"

void join_sort_preprocess(
  uint8_t *primary_rows, uint32_t primary_rows_len, uint32_t num_primary_rows,
  uint8_t *foreign_rows, uint32_t foreign_rows_len, uint32_t num_foreign_rows,
  uint8_t *output_rows, uint32_t output_rows_len, uint32_t *actual_output_len) {
  (void)primary_rows_len;
  (void)foreign_rows_len;
  (void)output_rows_len;

  // Set the row upper bound for the output rows to the max of the primary and foreign row sizes by
  // reading a row from each, converting it to the output format, and taking the max upper bound
  uint32_t row_upper_bound;
  {
    RowReader r1(primary_rows);
    RowReader r2(foreign_rows);
    NewRecord a;
    NewJoinRecord b;
    r1.read(&a);
    b.set(true, &a);
    row_upper_bound = b.row_upper_bound();
    debug("a upper bound %d\n", b.row_upper_bound());
    r2.read(&a);
    b.set(false, &a);
    debug("b upper bound %d\n", b.row_upper_bound());
    if (b.row_upper_bound() > row_upper_bound) {
      row_upper_bound = b.row_upper_bound();
    }
  }

  RowWriter w(output_rows, row_upper_bound);
  NewRecord a;
  NewJoinRecord b;

  RowReader primary(primary_rows);
  for (uint32_t i = 0; i < num_primary_rows; i++) {
    primary.read(&a);
    b.set(true, &a);
    w.write(&b);
  }

  RowReader foreign(foreign_rows);
  for (uint32_t i = 0; i < num_foreign_rows; i++) {
    foreign.read(&a);
    b.set(false, &a);
    w.write(&b);
  }

  w.close();
  *actual_output_len = w.bytes_written();
}

void scan_collect_last_primary(int op_code,
                               uint8_t *input_rows, uint32_t input_rows_length,
                               uint32_t num_rows,
                               uint8_t *output, uint32_t output_length,
                               uint32_t *actual_output_len) {
  (void)op_code;
  (void)input_rows_length;
  (void)output_length;

  RowReader r(input_rows);
  NewJoinRecord cur, last_primary;
  last_primary.reset_to_dummy();

  for (uint32_t i = 0; i < num_rows; i++) {
    r.read(&cur);
    if (cur.is_primary()) {
      last_primary.set(&cur);
    }
  }

  IndividualRowWriter w(output);
  w.write(&last_primary);
  w.close();
  *actual_output_len = w.bytes_written();
}

void process_join_boundary(int op_code,
                           uint8_t *input_rows, uint32_t input_rows_length,
                           uint32_t num_rows,
                           uint8_t *output_rows, uint32_t output_rows_size,
                           uint32_t *actual_output_length) {
  (void)op_code;
  (void)input_rows_length;
  (void)output_rows_size;

  IndividualRowReader r(input_rows);
  IndividualRowWriter w(output_rows);
  NewJoinRecord prev, cur;
  cur.reset_to_dummy();

  for (uint32_t i = 0; i < num_rows; i++) {
    prev.set(&cur);
    w.write(&prev);

    r.read(&cur);
    if (!cur.is_primary()) {
      cur.set(&prev);
    }
  }

  w.close();
  *actual_output_length = w.bytes_written();
}

void sort_merge_join(int op_code,
                     uint8_t *input_rows, uint32_t input_rows_length,
                     uint32_t num_rows,
                     uint8_t *join_row, uint32_t join_row_length,
                     uint8_t *output_rows, uint32_t output_rows_length,
                     uint32_t *actual_output_length) {
  (void)input_rows_length;
  (void)join_row_length;
  (void)output_rows_length;

  RowReader r(input_rows);
  RowWriter w(output_rows);
  NewJoinRecord primary, current;
  NewRecord dummy;
  NewRecord merge;

  IndividualRowReader j_reader(join_row);
  j_reader.read(&primary);
  if (!primary.is_dummy()) {
    check(primary.is_primary(), "sort_merge_join: join_row must be marked as primary\n");
    primary.init_join_attribute(op_code);
  }

  NewJoinRecord::init_dummy(&dummy, op_code);

  for (uint32_t i = 0; i < num_rows; i++) {
    r.read(&current);
    current.init_join_attribute(op_code);

    if (current.is_primary()) {
      check(!primary.join_attr_equals(&current, op_code),
            "sort_merge_join - primary table uniqueness constraint violation: multiple rows from "
            "the primary table had the same join attribute\n");
      primary.set(&current); // advance to a new join attribute
      w.write(&dummy);
    } else {
      if (!primary.join_attr_equals(&current, op_code)) {
        w.write(&dummy); // drop any foreign table rows without a matching primary table row
      } else {
        primary.merge(&current, &merge, op_code);
        w.write(&merge);
      }
    }
  }

  w.close();
  *actual_output_length = w.bytes_written();
  return;
}


void non_oblivious_sort_merge_join(int op_code,
								   uint8_t *input_rows, uint32_t input_rows_length,
								   uint32_t num_rows,
								   uint8_t *output_rows, uint32_t output_rows_length,
                                   uint32_t *actual_output_length, uint32_t *num_output_rows) {
  (void) input_rows_length;
  (void) output_rows_length;
  
  RowReader reader(input_rows);
  RowWriter writer(output_rows);

  NewJoinRecord primary, current;
  NewRecord merge;

  uint32_t num_output_rows_result = 0;
  for (uint32_t i = 0; i < num_rows; i++) {
    reader.read(&current);
    current.init_join_attribute(op_code);

    if (current.is_primary()) {
      check(!primary.join_attr_equals(&current, op_code),
            "sort_merge_join - primary table uniqueness constraint violation: multiple rows from "
            "the primary table had the same join attribute\n");
      primary.set(&current); // advance to a new join attribute
    } else {
      if (primary.join_attr_equals(&current, op_code)) {
        primary.merge(&current, &merge, op_code);
        writer.write(&merge);
        num_output_rows_result++;
      }
    }
  }

  writer.close();
  *actual_output_length = writer.bytes_written();
  *num_output_rows = num_output_rows_result;
}
