#include <abel/Process.hpp>

#include <utility>

#include <abel/Error.hpp>

namespace abel {

Process Process::create(
    const std::string &executable,
    const std::string &arguments,
    const std::string &workingDirectory,
    bool inheritHandles,
    DWORD creationFlags,
    DWORD startupFlags,
    Handle stdInput,
    Handle stdOutput,
    Handle stdError,
    std::function<void(STARTUPINFOA &)> extraParams
) {
    PROCESS_INFORMATION processInfo{};

    std::string fullArgs{};
    fullArgs.reserve(arguments.size() + executable.size() + 2);
    fullArgs.append(executable);
    fullArgs.push_back(' ');
    fullArgs.append(arguments);

    if (stdInput || stdOutput || stdError) {
        startupFlags |= STARTF_USESTDHANDLES;
    }

    STARTUPINFOA startupInfo{
        .cb = sizeof(STARTUPINFOA),
        .dwFlags = startupFlags,
        .hStdInput = stdInput.raw(),
        .hStdOutput = stdOutput.raw(),
        .hStdError = stdError.raw(),
    };

    if (extraParams) {
        extraParams(startupInfo);
    }

    bool success = CreateProcessA(
        executable.c_str(),
        fullArgs.data(),
        nullptr,
        nullptr,
        inheritHandles,
        creationFlags,
        nullptr,
        workingDirectory.size() > 0 ? workingDirectory.c_str() : nullptr,
        &startupInfo,
        &processInfo
    );

    if (!success) {
        fail("Failed to create process");
    }

    OwningHandle process{processInfo.hProcess};
    OwningHandle thread{processInfo.hThread};

    process.validate();
    thread.validate();

    Process result{};

    result.process = std::move(process);
    result.pid = processInfo.dwProcessId;
    result.thread = std::move(thread);
    result.tid = processInfo.dwThreadId;

    return result;
}

}  // namespace abel
