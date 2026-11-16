// strata/platform/dax.hpp: STRATA_DAX in the system's place, and a file that is not there.  No GPU, and no
// persistent memory: what the system says of a real file is not checked here, since the test's own folder may be
// on a DAX volume or not.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "strata/platform/dax.hpp"

using namespace strata::platform;

static int failed = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failed; } } while (0)

static void set_dax(const char* value) {
#if defined(_WIN32)
    _putenv_s("STRATA_DAX", value ? value : "");   // an empty value removes it
#else
    if (value) setenv("STRATA_DAX", value, 1);
    else unsetenv("STRATA_DAX");
#endif
}

int main(int, char** argv) {
    const std::string missing = "strata-dax-test-no-such-file.gguf", self = argv[0];

    set_dax(nullptr);
    CHECK(dax_override() == -1);
    CHECK(!system_says_dax(missing));                    // cannot be opened: no
    CHECK(!file_is_dax(missing));
    CHECK(file_is_dax(self) == system_says_dax(self));   // unset: the system's answer, whichever it is

    set_dax("1");
    CHECK(dax_override() == 1);
    CHECK(file_is_dax(missing));                         // every file, without asking the system
    CHECK(file_is_dax(self));
    CHECK(!system_says_dax(missing));                    // which is not changed by it

    set_dax("0");
    CHECK(dax_override() == 0);
    CHECK(!file_is_dax(missing));
    CHECK(!file_is_dax(self));

    set_dax("");                                         // empty is unset
    CHECK(dax_override() == -1);

    set_dax(nullptr);
    if (failed) return 1;
    std::printf("dax_test: ok\n");
    return 0;
}
