#define KNORM  "\x1B[0m"
#define KLRED "\x1B[91m"
#define KLGREEN "\x1B[92m"

#define FAIL(msg) do {                                        \
        cout << KLRED << "FAIL: " << msg << KNORM << endl;    \
        std::abort();                                         \
    } while (0);

