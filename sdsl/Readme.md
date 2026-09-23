`sdsl-lite` does not compile with current compilers. Both gcc 15 and clang 21 reject two
identifiers in `include/sdsl/louds_tree.hpp` (gcc 12 still accepts them):

```text
include/sdsl/louds_tree.hpp:182:51: error: no member named 'm_select1' in 'louds_tree<...>'
include/sdsl/louds_tree.hpp:183:51: error: no member named 'm_select0' in 'louds_tree<...>'
```

Inside `swap()`, `tree.m_select1` and `tree.m_select0` should read `tree.m_bv_select1`
and `tree.m_bv_select0`. Correcting those two names lets `./install.sh` complete. This is
a bug in `sdsl-lite`, not in this repository — it is tracked upstream as
[simongog/sdsl-lite#462](https://github.com/simongog/sdsl-lite/issues/462), open since
November 2024 against a repository whose last commit was in December 2019, so the edit
has to be made locally.

With CMake 4 or later, `install.sh` also stops at the configure step with "Compatibility
with CMake < 3.5 has been removed from CMake", because `sdsl-lite` declares
`cmake_minimum_required(VERSION 2.8.11)`. Raising the policy minimum gets past it:

```bash
CMAKE_POLICY_VERSION_MINIMUM=3.5 ./install.sh
```

