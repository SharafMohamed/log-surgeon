# log-surgeon: A performant log parsing library

[![CLP on Zulip](https://img.shields.io/badge/zulip-yscope--clp%20chat-1888FA?logo=zulip)](https://yscope-clp.zulipchat.com/)

`log-surgeon` is a library for high-performance parsing of unstructured text
logs. It allows users to parse and extract information from the vast amount of
unstructured logs generated by today's open-source software.

Some of the library's features include:

* Parsing and extracting variable values like the log event's log-level and any
  other user-specified variables, no matter where they appear in each log event.
* Parsing by using regular expressions for each variable type rather than
  regular expressions for an entire log event.
* Improved latency, and memory efficiency compared to popular regex engines.
* Parsing multi-line log events (delimited by timestamps).

Note that `log-surgeon` is *not* a generic regex engine and does impose [some
constraints](docs/parsing-constraints.md) on how log events can be parsed.

## Motivating example

Let's say we want to parse and inspect multi-line log events like this:
```
2023-02-23T18:10:14-0500 DEBUG task_123 crashed. Dumping stacktrace:
#0  0x000000000040110e in bar () at example.cpp:6
#1  0x000000000040111d in bar () at example.cpp:10
#2  0x0000000000401129 in main () at example.cpp:15
```

Using the [example schema file](examples/schema.txt) which includes these rules:
```
timestamp:\d{4}\-\d{2}\-\d{2}T\d{2}:\d{2}:\d{2}\-\d{4}
...
loglevel:INFO|DEBUG|WARN|ERROR
```

We can parse and inspect the events as follows:
```cpp
// Define a reader to read from your data source
Reader reader{/* <Omitted> */};

// Instantiate the parser
ReaderParser parser{"examples/schema.txt"};
parser.reset_and_set_reader(reader);

// Get the loglevel variable's ID
optional<uint32_t> loglevel_id{parser.get_variable_id("loglevel")};
// <Omitted validation of loglevel_id>

while (false == parser.done()) {
    if (ErrorCode err{parser.parse_next_event()}; ErrorCode::Success != err) {
        throw runtime_error("Parsing Failed");
    }

    // Get and print the timestamp
    Token* timestamp{event.get_timestamp()};
    if (nullptr != timestamp) {
        cout << "timestamp: " << timestamp->to_string_view() << endl;
    }

    // Get and print the log-level
    auto const& loglevels = event.get_variables(*loglevel_id);
    if (false == loglevels.empty()) {
        // In case there are multiple matches, just get the first one
        cout << "loglevel:" << loglevels[0]->to_string_view() << endl;
    }

    // Other analysis...

    // Print the entire event
    LogEventView const& event = parser.get_log_parser().get_log_event_view();
    cout << event->to_string() << endl;
}
```

For advanced uses, `log-surgeon` also has a
[BufferParser](examples/buffer-parser.cpp) that reads directly from a buffer.

## Building and installing

Requirements:

* CMake
* GCC >= 10 or Clang >= 7
* [Catch2] >= 3
  * On Ubuntu <= 20.04, you can install it using:
    ```shell
    sudo tools/deps-install/ubuntu/install-catch2.sh 3.6.0
    ```
  * On Ubuntu >= 22.04, you can install it using:
    ```shell
    sudo apt-get update
    sudo apt-get install catch2
    ```
  * On macOS, you can install it using:
    ```shell
    brew install catch2
    ```

From the repo's root, run:
```shell
# Generate the CMake project
cmake -S . -B build -DBUILD_TESTING=OFF
# Build the project
cmake --build ./build -j
# Install the project to ~/.local
cmake --install ./build --prefix ~/.local
```

To build the debug version and tests replace the first command with:
`cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`

## Documentation and examples

* [docs](docs) contains more detailed documentation including:
  * The [schema specification](docs/schema.md), which describes the syntax for
    writing your own schema
  * `log-surgeon`'s [design objectives](docs/design-objectives.md)
* [examples](examples) contains programs demonstrating usage of the library.

## Testing

To run unit tests, run:
```shell
cmake --build ./build --target test
```

## Linting

Before submitting a PR, ensure you've run our linting tools and either fixed any violations or
suppressed the warning.

### Requirements

We currently support running our linting tools on Linux and macOS. If you're developing on another
OS, you can submit a [feature request][feature-req]. If you can't run the linting workflows
locally, you can enable and run the [lint] workflow in your fork.

To run the linting tools, besides commonly installed tools like `tar`, you'll need:

* `md5sum`
* Python 3.8 or newer
* python3-venv
* [Task]

### Running the linters

To report all errors run:

```shell
task lint:check
```

To fix cpp errors, and report yml errors, run:

```shell
task lint:fix
```

## Providing feedback

You can use GitHub issues to [report a bug](https://github.com/y-scope/log-surgeon/issues/new?assignees=&labels=bug&template=bug-report.yml)
or [request a feature][feature-req].

Join us on [Zulip](https://yscope-clp.zulipchat.com/) to chat with developers
and other community members.

## Known issues

The following are issues we're aware of and working on:
* Schema rules must use ASCII characters. We will release UTF-8 support in a
  future release.
* Timestamps must appear at the start of the message to be handled specially
  (than other variable values) and support multi-line log events.
* A variable pattern has no way to match text around a variable, without having
  it also be a part of the variable.
  * Support for submatch extraction will be coming in a future release.

[Catch2]: https://github.com/catchorg/Catch2/tree/devel
[feature-req]: https://github.com/y-scope/log-surgeon/issues/new?assignees=&labels=enhancement&template=feature-request.yml
[lint]: https://github.com/y-scope/log-surgeon/blob/main/.github/workflows/lint.yml
[Task]: https://taskfile.dev/
