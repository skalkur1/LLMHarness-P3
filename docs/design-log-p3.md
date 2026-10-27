# Project 3 Design Log
**Author:** Shashank Kalkur · **P2 code used:** my own
**Tools used:** I used Claude to help me draft general program structures, plan implementation of the testing scripts, and understand stack implementation for calculator tool. I used ChatGPT to help with markdown formatting for design log.

## 1. Hash table

**Collision strategy: separate chaining.** Each bucket is the head of a singly
linked list of `Node { Entry e; std::unique_ptr<Node> next; }`, and `Entry`
stores the tool's name next to its `unique_ptr<Tool>`. New tools are linked at
the front of their bucket's list. I chose chaining over linear probing because
a collision only lengthens one list, there
is no wrap-around probing to get wrong, and I understand this approach better.

**Hash function: 64-bit FNV-1a.** It XORs each byte into the hash and then
multiplies by a prime, so it uses every character and depends on their order.
The bucket index is `hash & (bucket_count - 1)`, which equals the remainder
because the bucket count is always a power of two.

**Load factor and resizing.** The table starts with 8 buckets. Before inserting,
`add` checks whether one more entry would push `size / buckets` above 1.0. If
so, it doubles the bucket count and relinks every node into the bucket its hash
gives under the new count. The check happens before the insert, so the load
factor never exceeds 1.0 after `add` returns.

**Measurements (10,000 names `tool_0` … `tool_9999`):**

| Hash | bucket_count | load_factor | longest chain |
|---|---|---|---|
| FNV-1a (mine) | 16384 | 0.610 | **6** |
| `s[0] + s.size()` | 16384 | — | **9000** |

Every name starts with `'t'` and there are only four lengths, so the weak hash
puts all 10,000 names into 4 buckets. With FNV-1a the worst lookup checks 6.

**Why `add()` is amortized O(1).** Resizing from n to 2n buckets costs O(n), but
it happens only when the table is full, so the n inserts since the previous
resize pay for it. 

## 2. Pointer stability

`find` returns `entry.tool.get()`, the address of the `Tool` object itself. A
resize unlinks each `Node` from its old list and links it into a new one by
moving `unique_ptr`s. No `Tool` is copied or moved, and no `Node` is either,
because only pointers change. The `Tool` stays at the same address on the
heap, so any `Tool*` handed out earlier is still valid.
`reg_10000_tools` checks this: the pointer to `tool_0` from the first insert is
unchanged after 11 resizes.

## 3. Calculator trace

Expression `2 * (3 + 4) - 5`, stacks shown bottom to top:

1. `2` is a number, so it goes on `values`: `[2]`, ops `[]`.
2. `*`: nothing is waiting on `ops`, so `*` is pushed: ops `[*]`.
3. `(` is pushed as a barrier: ops `[* (]`.
4. `3` is pushed: values `[2 3]`.
5. `+`: the top of `ops` is `(`, so nothing is applied. Push: ops `[* ( +]`.
6. `4` is pushed: values `[2 3 4]`.
7. `)` closes the group. Apply `+`: pop 4 (the right operand), then 3, and
   push 7. The top is now `(`, so pop it. Values `[2 7]`, ops `[*]`.
8. `-`: the waiting `*` has precedence 2, which is at least that of `-` (1), so
   it can go first. Apply 2 × 7 = 14, then push `-`. Values `[14]`, ops `[-]`.
9. `5` is pushed: values `[14 5]`.
10. The tokens are finished. Apply `-`: 14 − 5 = 9. Values `[9]`.

The single value left, 9, is the answer.

## 4. Errors as values

In an agent loop a failing tool is expected. For example, if the model may ask
for a missing file or a path outside the workspace. The right response
is to tell the model what went wrong so it can try something else, and the
conversation should continue. Returning `false` with a message (or
`ToolResult::failure`) makes that the normal path: the message goes straight
back to the model as data. An uncaught exception would end the whole session
over one bad request.

As detailed by the spec, exceptions are for programmer errors, meaning bugs in C++ code, such as reading
a JSON number as a string. Those mean the program itself is wrong, so failing
loudly is useful.

## 5. What I would design differently

I would consider open addressing with linear probing instead of my chaining. My
table makes a separate heap allocation for every `Node`, and a lookup follows
`next` pointers that can be anywhere in memory. A flat array of entries would
need no per-tool allocation and would keep a probe sequence in neighbouring
memory, which is faster for the CPU cache. I only implemented the linked list
because I thought it would be simpler, but after some research linear probing could be faster.