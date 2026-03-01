# Data Model Specification

## 1. Overview

The logical file is represented as a doubly-linked list of **blocks**. Each block covers a contiguous range of the logical file and either references a region of the original file on disk (clean) or holds modified bytes (dirty). The concatenation of all blocks in list order is the logical file at any point in time.

This model supports:
- O(1) metadata-only inserts and deletes for clean blocks (no data copying)
- Bounded memory usage via on-demand loading and LRU eviction
- Safe dirty-block eviction to a temp file
- Atomic save via write-to-temp-then-rename

---

## 2. Block Structure

```cpp
enum class BlockState {
    CLEAN_UNLOADED,   // not in RAM; original file is the authoritative source
    CLEAN_LOADED,     // in RAM; content matches original file at [origin_offset, origin_offset+size)
    DIRTY_LOADED,     // in RAM; content has been modified or newly inserted
    DIRTY_UNLOADED,   // flushed to temp file at [temp_offset, temp_offset+size)
};

struct Block {
    BlockState       state;
    uint64_t         logical_size;        // number of bytes this block contributes to the logical file

    uint64_t         origin_file_offset;  // valid for CLEAN_UNLOADED and CLEAN_LOADED
    uint64_t         temp_file_offset;    // valid for DIRTY_UNLOADED

    std::vector<uint8_t> data;            // valid for CLEAN_LOADED and DIRTY_LOADED

    Block*           prev;
    Block*           next;

    uint64_t         last_access_tick;    // for LRU eviction ordering
};
```

A block is never both an original-file reference and a temp-file reference. The `origin_file_offset` and `temp_file_offset` fields are mutually exclusive by state.

---

## 3. Block List

```cpp
struct BlockList {
    Block*           head;
    Block*           tail;
    uint64_t         logical_file_size;   // sum of all block logical_sizes; updated on every structural change

    std::fstream     original_file;       // opened read-only on file open
    std::fstream     temp_file;           // opened read/write in system temp dir; created on first dirty eviction

    size_t           loaded_bytes;        // current total bytes in RAM across all loaded blocks
    size_t           memory_limit;        // maximum bytes allowed in RAM (default: 64 MB)
    uint64_t         access_tick;         // monotonic counter incremented on every block access
};
```

---

## 4. Block Size Policy

| Constant           | Value   | Purpose                                               |
|--------------------|---------|-------------------------------------------------------|
| `TARGET_BLOCK_SIZE`| 64 KB   | Preferred size for new clean blocks on load           |
| `MAX_BLOCK_SIZE`   | 128 KB  | Blocks exceeding this are split before in-place edits |
| `MIN_BLOCK_SIZE`   | 512 B   | Blocks below this are merge candidates                |
| `TINY_BLOCK_SIZE`  | 64 B    | Blocks below this trigger an immediate merge attempt  |

These constants are defined in code and not user-configurable at runtime.

---

## 5. Initialization

On file open, the block list is initialized with a single CLEAN_UNLOADED block spanning the entire file:

```
[CLEAN_UNLOADED | origin=0 | size=file_size]
```

No data is loaded. Block metadata for this state is small regardless of file size (one struct allocation).

When bytes in a region need to be read (for display or editing), the covering block is split into TARGET_BLOCK_SIZE-aligned CLEAN_UNLOADED blocks, and the required ones are loaded. Splitting a CLEAN_UNLOADED block is a pure metadata operation with no I/O.

---

## 6. Logical Offset Lookup

There is no per-block cached logical offset. To find the block containing logical offset `X`:

1. Start from the head (or from a **viewport anchor** — see §6.1).
2. Walk forward, accumulating block sizes, until the accumulated size exceeds `X`.
3. Return the block and the intra-block offset `x = X - accumulated_before_block`.

This is O(n) in block count, which is acceptable because the number of blocks in a typical session is small (hundreds to low thousands), and most accesses are near the viewport.

### 6.1 Viewport Anchor

The block list maintains a viewport anchor:

```cpp
struct ViewportAnchor {
    Block*   block;               // block at the top of the visible area
    uint64_t block_logical_start; // precomputed logical offset of that block's first byte
};
```

The anchor is updated whenever the viewport scrolls. Lookups for offsets near the viewport start from the anchor instead of the head, making them effectively O(1) for all on-screen operations.

When the anchor block is removed (due to deletion or merge), the anchor is reset to the nearest surviving block.

---

## 7. Loading a Block

Before reading from or writing to a block's data, it must be loaded.

### Loading CLEAN_UNLOADED

1. If `logical_size > TARGET_BLOCK_SIZE`, split the block first (§9.1) until all sub-blocks are ≤ TARGET_BLOCK_SIZE. Only the sub-block(s) that are actually needed are then loaded.
2. Check memory budget. If `loaded_bytes + logical_size > memory_limit`, evict blocks first (§8).
3. Allocate `data` buffer of `logical_size` bytes.
4. Read `logical_size` bytes from `original_file` at `origin_file_offset`.
5. Set `state = CLEAN_LOADED`.
6. Add `logical_size` to `loaded_bytes`.
7. Update `last_access_tick`.

### Loading DIRTY_UNLOADED

Same as above, except read from `temp_file` at `temp_file_offset` in step 4, and set `state = DIRTY_LOADED`.

---

## 8. Memory Management and Eviction

### 8.1 Eviction Trigger

Before loading a block, if the load would exceed `memory_limit`, evict blocks until sufficient headroom exists. Evict in ascending order of `last_access_tick` (least recently accessed first), skipping any block currently in use by an active read or write operation.

Blocks near the viewport (within 2 blocks of the anchor in either direction) are never evicted.

### 8.2 Evicting a CLEAN_LOADED Block

Clean blocks can be evicted for free — their content can always be re-read from the original file:

1. Clear `data` buffer.
2. Set `state = CLEAN_UNLOADED` (`origin_file_offset` is already valid).
3. Subtract `logical_size` from `loaded_bytes`.

### 8.3 Evicting a DIRTY_LOADED Block

Dirty blocks must be flushed to the temp file before eviction:

1. Open `temp_file` if not already open (create in system temp directory, e.g. `/tmp/hexedit_XXXXXX`).
2. Seek to end of `temp_file`.
3. Record current position as `temp_file_offset` for this block.
4. Write `data` buffer to `temp_file`.
5. Flush.
6. Clear `data` buffer.
7. Set `state = DIRTY_UNLOADED`.
8. Subtract `logical_size` from `loaded_bytes`.

The temp file is append-only during the session. Flushed regions are never reclaimed mid-session (space is recovered only on session close or save). This means a block evicted and reloaded multiple times will accumulate multiple regions in the temp file; only the most recent `temp_file_offset` is ever referenced.

---

## 9. Structural Operations

### 9.1 Splitting a Block

Splitting divides one block into two adjacent blocks at intra-block offset `x`.

**CLEAN block (either state)** — pure metadata, no I/O required:
```
Original: [CLEAN | origin=O | size=S]
After split at x:
  [CLEAN | origin=O     | size=x    ]
  [CLEAN | origin=O+x   | size=S-x  ]
```
Both halves retain their original state (CLEAN_UNLOADED or CLEAN_LOADED as appropriate). If the original had a loaded data buffer, the halves get sub-slices of it.

**DIRTY_LOADED block** — data must be split in memory:
```
left.data  = block.data[0 .. x)
right.data = block.data[x .. S)
```
Both halves are DIRTY_LOADED.

**DIRTY_UNLOADED block** — load first (§7), then split as DIRTY_LOADED.

After splitting, update the linked list pointers and `logical_file_size` is unchanged (sum of halves equals original size).

### 9.2 Merging Two Adjacent Blocks

Two adjacent blocks `L` (left) and `R` (right) can be merged if:
- Their combined size does not exceed `MAX_BLOCK_SIZE`.
- Both are merge candidates (individually below `MIN_BLOCK_SIZE`), OR a merge is explicitly requested.

| L state         | R state         | Merge action                                                        | Result state    |
|-----------------|-----------------|---------------------------------------------------------------------|-----------------|
| CLEAN_UNLOADED  | CLEAN_UNLOADED  | Only if contiguous in original file (R.origin == L.origin+L.size). New block spans both. | CLEAN_UNLOADED  |
| CLEAN_LOADED    | CLEAN_LOADED    | Concatenate data buffers. Only if contiguous.                       | CLEAN_LOADED    |
| CLEAN (any)     | CLEAN (any)     | Non-contiguous: load both if needed, copy data, result is DIRTY.    | DIRTY_LOADED    |
| DIRTY_LOADED    | DIRTY_LOADED    | Concatenate data buffers.                                            | DIRTY_LOADED    |
| DIRTY_UNLOADED  | DIRTY_UNLOADED  | Load both first (§7), then merge as DIRTY_LOADED.                   | DIRTY_LOADED    |
| DIRTY (any)     | CLEAN (any)     | Load both if needed, concatenate, result is DIRTY.                  | DIRTY_LOADED    |
| CLEAN (any)     | DIRTY (any)     | Same as above.                                                       | DIRTY_LOADED    |

After merging, remove one block from the list, update the surviving block's fields, and update `logical_file_size` (unchanged — size of merged block equals sum of parts).

### 9.3 Merge Trigger

After any insert or delete operation:
1. Collect the directly affected blocks and their immediate neighbors.
2. For any block below `TINY_BLOCK_SIZE`, attempt to merge with both neighbors immediately.
3. For any block below `MIN_BLOCK_SIZE`, schedule a deferred merge (next idle tick).
4. Never merge if the result would exceed `MAX_BLOCK_SIZE`.

---

## 10. Insert Operation

To insert `N` bytes of new content at logical offset `X`:

1. Find block `B` containing `X` (§6). Let `x` be the intra-block offset.
2. **If `x == 0`**: insert new block immediately before `B` in the list. No split needed.
3. **If `x == B.logical_size`**: insert new block immediately after `B` in the list. No split needed.
4. **Otherwise**:
   - Split `B` at `x` into `B_left` and `B_right` (§9.1). `B` is removed.
   - Insert new block between `B_left` and `B_right`.
5. The new block has `state = DIRTY_LOADED`, `data = new_content`.
6. Add `N` to `logical_file_size`.
7. Run merge check (§9.3) on the new block and neighbors.
8. Update group offsets (§12.1).
9. Record undo entry (§13).

For CLEAN blocks, step 4 never requires loading data — the split is metadata-only. The new block is the only allocation. This keeps insert cost low even for large clean blocks.

---

## 11. Delete Operation

To delete `N` bytes starting at logical offset `X`:

1. Find block `B_start` containing `X` and intra-block offset `x_start`.
2. Find block `B_end` containing `X + N - 1` and intra-block offset `x_end`.
3. **Case: single block** (`B_start == B_end`):
   - If the deletion covers the entire block (`x_start == 0 && N == B_start.logical_size`): remove block from list.
   - Otherwise: for CLEAN blocks, adjust `origin_file_offset` and `logical_size` without loading. For DIRTY blocks, load if needed and erase bytes `[x_start, x_start+N)` from `data`.
4. **Case: multiple blocks**:
   a. Trim `B_start`: remove bytes `[x_start, B_start.logical_size)` from `B_start`.
      - CLEAN: adjust size (no load needed). DIRTY: load and truncate data.
   b. Remove all blocks strictly between `B_start` and `B_end` from the list entirely.
   c. Trim `B_end`: remove bytes `[0, x_end+1)` from `B_end`.
      - CLEAN: adjust `origin_file_offset += (x_end+1)` and `logical_size -= (x_end+1)`. DIRTY: load and erase prefix.
5. If trimming produced a zero-size block, remove it.
6. Subtract `N` from `logical_file_size`.
7. Run merge check (§9.3) on surviving neighbors of the deletion.
8. Update group offsets (§12.2).
9. Record undo entry (§13).

---

## 12. Group System

Groups are logical annotations. They hold no block references and do not affect the block list. Their position in the file is expressed as a byte offset relative to their parent group's start.

```cpp
enum class GroupType { PRIMITIVE, STRING, ARRAY, REFERENCE, TEMPLATE };

struct Group {
    std::string      name;
    Color            color;
    GroupType        type;
    TypeParams       type_params;     // endianness, encoding, element type, etc.

    uint64_t         parent_relative_offset;  // offset from start of parent group
    uint64_t         base_size;               // nominal byte span; for variable-length types, this is the maximum

    Group*           parent;
    std::vector<Group*> children;
};
```

The absolute file offset of a group is:

```
absolute_offset(root) = 0
absolute_offset(g)    = g.parent_relative_offset + absolute_offset(g.parent)
```

### 12.1 Group Offset Update on Insert

When `N` bytes are inserted at absolute file offset `X`:

Walk the group tree. For every group `g` whose `absolute_offset(g) >= X`, increment `g.parent_relative_offset` by `N`. (The increment is applied to `parent_relative_offset`, not to the absolute offset directly, so that only the minimal set of groups need updating.)

In practice: a group starting at absolute offset `A` needs updating only if `A >= X`. The update can be scoped efficiently by noting which subtrees start before vs. after `X`.

Groups whose byte range straddles `X` (start before, end after) are unaffected in their start offset but their `base_size` should be increased by `N` to reflect that the insertion is within them.

### 12.2 Group Offset Update on Delete

When `N` bytes are deleted starting at absolute file offset `X`:

- Groups starting at `>= X + N`: offset decreases by `N`.
- Groups starting in `[X, X+N)`: their start byte was deleted. Mark these as **orphaned** (display with a warning indicator; do not delete automatically so the user can recover them).
- Groups starting before `X` but ending inside `[X, X+N)`: their effective size shrinks. Update `base_size` accordingly.

### 12.3 Groups in Fixed-Size Mode

In fixed-size mode (overwrite), the logical file size never changes. Group offsets never need updating from byte operations (only from explicit user group edits).

---

## 13. Undo / Redo

The undo system operates on two levels: **byte-level** (block list changes) and **structural** (group tree changes). Both levels share one undo stack, with operations interleaved in chronological order.

```cpp
enum class UndoOpType {
    INSERT,           // byte insert
    DELETE,           // byte delete
    OVERWRITE,        // byte overwrite (fixed-size mode)
    GROUP_CREATE,
    GROUP_DELETE,
    GROUP_MODIFY,     // rename, recolor, retype, reparent, resize
};

struct UndoOp {
    UndoOpType       type;
    uint64_t         offset;          // for byte ops
    std::vector<uint8_t> data;        // bytes removed (for DELETE undo) or overwritten (for OVERWRITE undo)
    uint64_t         count;           // for INSERT undo (number of bytes to re-delete)
    GroupSnapshot    group_before;    // for GROUP_MODIFY undo
    GroupSnapshot    group_after;     // for GROUP_MODIFY redo
};
```

Undo of a DELETE replays an INSERT at the same offset with the recorded bytes. Undo of an INSERT replays a DELETE. This means byte-level undo always goes through the same insert/delete codepath, which correctly updates group offsets.

Consecutive overwrites to the same byte or byte run are coalesced into a single undo entry.

---

## 14. Saving

### 14.1 Save to New File

1. Open output file for writing.
2. Walk the block list from head to tail.
3. For each block:
   - `CLEAN_UNLOADED`: read `logical_size` bytes from `original_file` at `origin_file_offset`; write to output.
   - `CLEAN_LOADED`: write `data` to output.
   - `DIRTY_LOADED`: write `data` to output.
   - `DIRTY_UNLOADED`: read `logical_size` bytes from `temp_file` at `temp_file_offset`; write to output.
4. Flush and close output file.

### 14.2 Save to Original File (Overwrite)

Cannot read from and write to the same file simultaneously (CLEAN_UNLOADED blocks read from the original). Use a safe-save procedure:

1. Perform Save to New File (§14.1) using a temp path in the same directory as the original (same filesystem, enabling atomic rename).
2. `rename(temp_path, original_path)` — atomic on POSIX.
3. Post-save cleanup:
   - Reopen `original_file` pointing to the renamed file.
   - For every block in the list, compute its new `origin_file_offset` (its logical offset in the now-saved file).
   - Set all blocks to `CLEAN` state (their content is now the saved file). Evict from memory as needed; they are now re-loadable from the new original.
   - Close and delete `temp_file`. Clear `temp_file_offset` on all blocks.
   - Clear the undo stack (post-save undo would require re-dirtying the saved file, which is out of scope).

### 14.3 Error Handling

If the write in step 3 of §14.1 fails (disk full, permission error):
- Delete the incomplete temp output file.
- Report error to the user.
- The original file and block list are untouched.

---

## 15. Session Close

On close without saving:
- Delete `temp_file` if it exists.
- Free all block data buffers.
- Free all block structs.
- Free group tree.

On close with unsaved changes, prompt the user before performing the above.

---

## 16. Summary of Invariants

1. `logical_file_size` always equals the sum of `logical_size` across all blocks.
2. For a CLEAN block, `origin_file_offset + logical_size <= original_file_size_at_open`.
3. For a CLEAN block that is contiguous with its CLEAN predecessor: `predecessor.origin + predecessor.size == block.origin`. (Non-contiguous adjacent CLEAN blocks are legal; they arise from inserting/deleting bytes that split file regions.)
4. No block has `logical_size == 0` (zero-size blocks are immediately removed).
5. `loaded_bytes` equals the sum of `logical_size` for all CLEAN_LOADED and DIRTY_LOADED blocks.
6. `loaded_bytes <= memory_limit` except transiently during a load operation before eviction completes.
7. `absolute_offset(g) + g.base_size <= logical_file_size` for all non-orphaned groups.
