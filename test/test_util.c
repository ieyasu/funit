#include "../funit.h"
#include "../util.c"

static void test_string_buffer()
{
    struct StringBuffer sb = {(char *)0xDEADBEEF, 0, 0};

    sb_init(&sb, 4);
    assert(sb.s != NULL);
    assert(sb.cap == 4);
    assert(sb.len == 0);

    // make sure we can access the full length expected
    for (size_t i = 0; i < 4; i++) {
        sb.s[i] = '-';
    }
    for (size_t i = 0; i < 4; i++) {
        assert(sb.s[i] == '-');
    }

    // grow and test access to new space
    sb_ensure(&sb, 7);
    assert(sb.cap >= 7);
    assert(sb.len == 0);
    for (size_t i = 0; i < 7; i++) {
        sb.s[i] = '-';
    }
    for (size_t i = 0; i < 7; i++) {
        assert(sb.s[i] == '-');
    }

    sb_add_char(&sb, 'c');
    assert(sb.len == 1);
    assert(sb.s[0] == 'c');
    assert(sb.s[1] == '-');

    sb_add_char(&sb, 'd');
    assert(sb.len == 2);
    assert(sb.s[0] == 'c');
    assert(sb.s[1] == 'd');
    assert(sb.s[2] == '-');

    sb_add_nstr(&sb, "test", 4);
    assert(sb.len == 6);
    assert(strncmp(sb.s, "cdtest-", 7) == 0);

    sb.len = 0;
    sb_add_str(&sb, "four");
    sb_add_char(&sb, '\0');
    assert(sb.len == 5);
    assert(strcmp(sb.s, "four") == 0);

    sb_free(&sb);
}

int main(int argc, char **argv)
{
    test_string_buffer();

    puts("all util tests passed!");
}
