# Echo | Modern Hex Editor

A hex editor application written in C++ using Qt for user interface, with modern features and a futuristic aesthetic.

## Overview

This editor runs natively on supported systems and can open any file, displaying a central hex view and side panels to view, analyze, interpret, and modify the data. In terms of core features, this software is similar to popular hexadecimal editor HxD. The main difference is its more modern, streamlined user interface.

## Features

### Basic Features

* The software can be used to open a file, displaying its contents with a hex view and an adjacent ASCII view. Bytes are shown as two uppercase hex digits. Non-printable bytes are shown as a dot in the ASCII view.
* An offset column to the left of the hex view shows the absolute file offset of the first byte on each row, in hexadecimal.
* For large files, the software does not load the entire file into memory. See `DATA_MODEL.md` for the full description of the paging and block system.
* **Edit mode** — by default, the editor does not enter edit mode on a plain keystroke (to leave room for keyboard shortcuts). A toggleable **direct edit switch** enables HxD-style behavior where typing immediately edits the selected byte(s) without a separate activation step.
  * When not in direct edit mode, a byte or ASCII character is activated for editing by double-clicking it, or by pressing Enter with it selected.
  * In the hex view, editing a byte requires typing two hex digits. After the second digit the cursor advances to the next byte automatically.
  * In the ASCII view, typing a printable character sets the byte to its ASCII value and the cursor advances.
  * Backspace moves the cursor back one byte (in fixed-size mode it does not modify the byte; in insert/delete mode it deletes the previous byte).
* **Selection** — a single click selects the byte under the cursor. Click-and-drag or Shift+Click extends the selection to a contiguous byte range. Shift+Arrow keys extend or shrink the selection one byte at a time. Ctrl+A selects the entire file.
* A right side panel interprets the selected bytes as various common types (signed/unsigned integers of 1, 2, 4, 8 bytes; 32-bit and 64-bit IEEE floats; raw binary) in both big-endian and little-endian. The displayed values update live as the selection changes.
* **Fixed-size mode vs. insert/delete mode** — the editor can be toggled between these two modes:
  * In **fixed-size mode**, the file length never changes. Typing and pasting overwrite existing bytes. Cut copies the selected bytes to the clipboard and then zeros them. Delete zeros the selected bytes in place without affecting the clipboard. Pasting overwrites bytes starting at the cursor.
  * In **insert/delete mode**, typing and pasting insert new bytes at the cursor, offsetting all subsequent bytes. Delete and Backspace remove bytes from the file entirely, reducing its length. Cut removes the bytes and places them on the clipboard.
* Changes can be undone and redone. Saves write to the original file using a safe write-to-temp-then-rename procedure, or can be saved as a new file.


### Node Hierarchy

Every byte in the file belongs to the **root node** (the file itself). Users can create named **child nodes** covering any sub-range of bytes within their parent, building up an arbitrary tree of fixed-size annotated regions. This tree is the primary tool for describing and navigating the structure of a binary file.

#### Node Model

* Every node has a **start offset** (relative to its parent's start) and a **fixed length** in bytes. Both are set at creation time and can be changed explicitly by the user; they are never adjusted automatically.
* A node's byte range must always be fully contained within its parent's byte range. This invariant is enforced on all operations.
* Sibling nodes may overlap (e.g. to represent a C union). There is no automatic prevention of overlapping siblings.
* The root node's length equals the file size and updates when the file is modified in insert/delete mode.

#### Leaves and Non-Leaves

* A **leaf node** is a node with no children. Leaf nodes must have a type (see Node Types below).
* A **non-leaf node** is a node with at least one child. Non-leaf nodes do not have a type; they are purely structural containers.
* If a non-leaf node becomes a leaf (its last child is removed or reparented away), it automatically defaults to the **blob** type.
* If a leaf node gains its first child, its type is cleared.

#### Node Types (Leaf Nodes Only)

* **Blob** — raw bytes with no further interpretation. The hierarchy view shows the node's name and byte length. This is the default type for newly created leaf nodes.
* **Primitives** — a fixed-width numeric value:
  * Signed integers: `i8`, `i16`, `i32`, `i64`
  * Unsigned integers: `u8`, `u16`, `u32`, `u64`
  * IEEE 754 floats: `f32`, `f64`
  * The interpreted value is shown in the hierarchy view.
* **Reference** — stores an integer offset that points to a location within a designated **base node**. A reference node has:
  * A **base node**, stored by unique node ID (renaming the base node does not invalidate the reference).
  * An optional **constant offset** added to the stored value.
  * The resolved address is `base_node.absolute_start + stored_value + constant_offset`.
  * The hierarchy view shows the resolved address and, if it falls within a named node, that node's name.
  * If the resolved address falls outside the file the reference is shown as invalid.

#### Representing Structures and Arrays

There are no dedicated struct or array types. These patterns are represented directly in the node tree:

* **Struct** — a non-leaf node whose children each cover a named field within the parent's byte range.
* **Array** — a non-leaf parent node with one uniformly-typed, equal-length leaf child per element. A mechanism for creating array elements automatically from a format specification will be added later.

#### Endianness

* Endianness is a per-node property. When a node is created it inherits its parent's endianness. The root node has an explicitly set endianness (default: little-endian).
* Changing a node's endianness does not affect its children unless explicitly propagated.
* Endianness is meaningful only for numeric primitive and reference leaf nodes; it is stored on all nodes for inheritance purposes.

#### Node Operations

* **Create** — select a byte range within a node and create a child node covering that range. The new node defaults to blob type and prompts for a name.
* **Rename** — change a node's display name. Node IDs used by references are unaffected.
* **Recolor** — assign a display color. If no color is assigned the node inherits neutral gray.
* **Retype** — change the type of a leaf node.
* **Resize / move** — change the node's start offset or length, subject to the containment constraint.
* **Reparent** — move a node to a new parent, subject to the containment constraint:
  * If the new parent starts before the node but ends before the node ends, the user is prompted to extend the new parent to include the node, or cancel.
  * If the new parent starts after the node's start offset, the operation fails with an error.
* **Merge into parent** — remove the node and promote its children (if any) to the parent, or simply remove a leaf node. The byte range it covered in the parent is retained.
* All node operations are undoable.

#### Coloring and Display

* Each node can be assigned a display color. This color tints the corresponding bytes in the hex and ASCII views.
* Bytes belonging only to the root node (no named descendant) are shown in neutral gray.
* When a byte is covered by multiple nodes at different depths, the **deepest node** takes precedence for coloring.
* When two nodes at the **same depth** both cover a byte (overlapping siblings), their colors are **additively blended**, capped at white.
* In **node selection mode** (togglable switch), clicking a byte selects the deepest node that contains it and highlights all bytes belonging to that node. If two nodes at the same depth both contain the clicked byte, the selection among them is undefined.

#### Parsed View

* In **parsed view**, the hex/ASCII view is replaced by a structured display of the node tree. Each node is shown with its name and — for typed leaf nodes — its interpreted value. Blob nodes show their byte length.
* Editing a value in parsed view modifies the underlying bytes.
* Parsed view and hex view can be toggled at any time.

#### Undo / Redo

* Undo and redo cover all editor operations: byte insertions, deletions, overwrites, node creation, deletion, renaming, recoloring, retyping, reparenting, and resizing.
* Undo history is unlimited within a session and is cleared on save.


## UI

### Overview

The layout is similar to standard hex editors like HxD: the hex/ASCII view in the center, side panels on the left and right, a menu bar (File, Edit, Select, View, …) at the top, and a row of mode switches/buttons below the menu bar.

### Left Panel: Node Hierarchy

The left panel has two panes:
* **Top pane** — the node tree. The root node represents the file. Nodes are shown with their name, type (for leaves), and a color swatch if assigned. Selecting a node highlights the corresponding bytes in the hex view.
* **Bottom pane** — details for the selected node: name, type (leaves only), byte length, start offset relative to parent, absolute offset, endianness, and color. For reference nodes, the resolved target is also shown.

A **Select bytes** button in the bottom pane selects the node's byte range in the hex view.

### Right Panel: Selected Bytes

The right panel shows:
* The absolute offset and byte length of the current selection.
* All nodes whose byte range overlaps the selection, listed by depth.
* Interpreted values of the selected bytes as all supported primitive types in both endiannesses.
* A **Create Node** button that creates a new blob child node from the current selection, inheriting the endianness of the deepest containing node, and immediately opens a name prompt.

### Aesthetic

The aesthetic is based on futuristic, high-contrast, flat, dark user interfaces. Controls use shades of gray and white; color is reserved for information display (node colors, selection highlights). No corners are rounded. The dominant background color is black.

The app implements a custom title bar with its own close, maximize, and minimize buttons, while remaining movable and resizable. On platforms where a custom title bar is not reliably supported (e.g. Wayland compositors that do not allow client-side decorations), the application falls back to the system-provided title bar.

Inspirations include UIs seen in movies, the Bloomberg Terminal, and Palantir software. Reference images are included in the `design_refs` directory.
