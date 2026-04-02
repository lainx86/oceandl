#!/usr/bin/env python3

from __future__ import annotations

import argparse
import contextlib
import dataclasses
import http.server
import os
import pathlib
import shutil
import subprocess
import sys
import threading
import traceback
import urllib.parse


DATASET_ID = "gpcp"
FILE_NAME = "precip.mon.mean.nc"
RESOURCE_BASE_PATH = f"/fixtures/{DATASET_ID}"
RESOURCE_PATH = f"{RESOURCE_BASE_PATH}/{FILE_NAME}"
ETAG = '"fixture-v1"'
PARTIAL_SIZE = 16
FIXTURE_BYTES = b"CDF\x01" + (b"\x00" * 60)


class IntegrationFailure(RuntimeError):
    pass


@dataclasses.dataclass
class RequestRecord:
    method: str
    path: str
    range_header: str | None
    if_range_header: str | None


@dataclasses.dataclass
class CommandResult:
    name: str
    argv: list[str]
    returncode: int
    stdout: str
    stderr: str


class RequestLog:
    def __init__(self) -> None:
        self._records: list[RequestRecord] = []
        self._lock = threading.Lock()

    def append(self, record: RequestRecord) -> None:
        with self._lock:
            self._records.append(record)

    def clear(self) -> None:
        with self._lock:
            self._records.clear()

    def snapshot(self) -> list[RequestRecord]:
        with self._lock:
            return list(self._records)


class HermeticHTTPServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(
        self,
        server_address: tuple[str, int],
        request_log: RequestLog,
    ) -> None:
        super().__init__(server_address, HermeticRequestHandler)
        self.request_log = request_log


class HermeticRequestHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "oceandl-hermetic-http"

    def do_HEAD(self) -> None:
        self._record_request()
        if self._request_path() != RESOURCE_PATH:
            self._send_not_found()
            return

        self.send_response(200)
        self.send_header("Content-Length", str(len(FIXTURE_BYTES)))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("ETag", ETAG)
        self.end_headers()

    def do_GET(self) -> None:
        self._record_request()
        if self._request_path() != RESOURCE_PATH:
            self._send_not_found()
            return

        range_header = self.headers.get("Range")
        if not range_header:
            self.send_response(200)
            self.send_header("Content-Length", str(len(FIXTURE_BYTES)))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("ETag", ETAG)
            self.end_headers()
            self.wfile.write(FIXTURE_BYTES)
            return

        try:
            start = self._parse_open_ended_range(range_header, len(FIXTURE_BYTES))
        except IntegrationFailure as error:
            payload = f"{error}\n".encode("utf-8")
            self.send_response(400)
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        body = FIXTURE_BYTES[start:]
        self.send_response(206)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Range", f"bytes {start}-{len(FIXTURE_BYTES) - 1}/{len(FIXTURE_BYTES)}")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("ETag", ETAG)
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format: str, *args: object) -> None:
        del format
        del args

    def _request_path(self) -> str:
        return urllib.parse.urlsplit(self.path).path

    def _record_request(self) -> None:
        self.server.request_log.append(
            RequestRecord(
                method=self.command,
                path=self._request_path(),
                range_header=self.headers.get("Range"),
                if_range_header=self.headers.get("If-Range"),
            )
        )

    def _send_not_found(self) -> None:
        payload = b"not found\n"
        self.send_response(404)
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    @staticmethod
    def _parse_open_ended_range(value: str, size: int) -> int:
        if not value.startswith("bytes=") or "," in value:
            raise IntegrationFailure(f"unexpected Range header format: {value}")

        range_value = value[len("bytes="):]
        start_text, dash, end_text = range_value.partition("-")
        if dash != "-" or not start_text or end_text:
            raise IntegrationFailure(f"expected open-ended Range header, got: {value}")

        start = int(start_text)
        if start < 0 or start >= size:
            raise IntegrationFailure(f"Range start is outside the fixture payload: {value}")
        return start


@contextlib.contextmanager
def running_server(request_log: RequestLog) -> HermeticHTTPServer:
    server = HermeticHTTPServer(("127.0.0.1", 0), request_log)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server
    finally:
        server.shutdown()
        thread.join(timeout=10)
        server.server_close()


def write_config(config_path: pathlib.Path, port: int) -> None:
    config_path.write_text(
        "\n".join(
            (
                f'default_dataset = "{DATASET_ID}"',
                "[dataset_base_urls]",
                f'{DATASET_ID} = "http://127.0.0.1:{port}{RESOURCE_BASE_PATH}"',
                "",
            )
        ),
        encoding="utf-8",
    )


def write_resume_fixture(output_dir: pathlib.Path) -> None:
    dataset_dir = output_dir / DATASET_ID
    dataset_dir.mkdir(parents=True, exist_ok=True)

    part_path = dataset_dir / f"{FILE_NAME}.part"
    part_path.write_bytes(FIXTURE_BYTES[:PARTIAL_SIZE])

    meta_path = dataset_dir / f"{FILE_NAME}.part.meta"
    meta_path.write_text(
        "\n".join(
            (
                f"content_length {len(FIXTURE_BYTES)}",
                f"etag {quoted(ETAG)}",
                f'last_modified {quoted("")}',
                "",
            )
        ),
        encoding="utf-8",
    )


def quoted(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def run_command(
    name: str,
    oceandl_path: pathlib.Path,
    config_path: pathlib.Path,
    output_dir: pathlib.Path,
) -> CommandResult:
    command = [
        str(oceandl_path),
        "--config",
        str(config_path),
        "--verbose",
        "download",
        DATASET_ID,
        "--output-dir",
        str(output_dir),
        "--chunk-size",
        "1024",
        "--timeout",
        "5",
        "--retries",
        "0",
    ]

    env = os.environ.copy()
    for key in (
        "http_proxy",
        "https_proxy",
        "HTTP_PROXY",
        "HTTPS_PROXY",
        "ALL_PROXY",
        "all_proxy",
    ):
        env.pop(key, None)
    env["NO_PROXY"] = "127.0.0.1,localhost"
    env["no_proxy"] = "127.0.0.1,localhost"
    env["NO_COLOR"] = "1"

    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
        env=env,
    )
    return CommandResult(
        name=name,
        argv=command,
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise IntegrationFailure(message)


def require_command_success(result: CommandResult) -> None:
    require(
        result.returncode == 0,
        f"{result.name} exited with {result.returncode}",
    )


def require_file_contents(path: pathlib.Path, expected: bytes) -> None:
    require(path.exists(), f"expected file to exist: {path}")
    require(path.read_bytes() == expected, f"unexpected file contents: {path}")


def assert_first_download(result: CommandResult, records: list[RequestRecord], output_dir: pathlib.Path) -> None:
    final_path = output_dir / DATASET_ID / FILE_NAME
    require_file_contents(final_path, FIXTURE_BYTES)
    require(
        [record.method for record in records] == ["HEAD", "GET"],
        f"unexpected request sequence for first download: {records}",
    )
    require(
        all(record.path == RESOURCE_PATH for record in records),
        f"unexpected request path during first download: {records}",
    )
    require(records[1].range_header is None, "first download unexpectedly used Range")
    require(
        f"Downloaded {FILE_NAME}" in result.stdout,
        "first download output did not report a successful transfer",
    )


def assert_skip_rerun(result: CommandResult, records: list[RequestRecord], output_dir: pathlib.Path) -> None:
    final_path = output_dir / DATASET_ID / FILE_NAME
    require_file_contents(final_path, FIXTURE_BYTES)
    require(
        [record.method for record in records] == ["HEAD"],
        f"unexpected request sequence for skip rerun: {records}",
    )
    require(
        f"Skipping {FILE_NAME}" in result.stdout,
        "skip rerun output did not explain that the file was reused",
    )


def assert_resume_download(
    result: CommandResult,
    records: list[RequestRecord],
    output_dir: pathlib.Path,
) -> None:
    dataset_dir = output_dir / DATASET_ID
    final_path = dataset_dir / FILE_NAME
    part_path = dataset_dir / f"{FILE_NAME}.part"
    meta_path = dataset_dir / f"{FILE_NAME}.part.meta"

    require_file_contents(final_path, FIXTURE_BYTES)
    require(not part_path.exists(), f"partial file should be removed after resume: {part_path}")
    require(not meta_path.exists(), f"partial metadata should be removed after resume: {meta_path}")
    require(
        [record.method for record in records] == ["HEAD", "GET"],
        f"unexpected request sequence for resume: {records}",
    )
    require(
        records[1].range_header == f"bytes={PARTIAL_SIZE}-",
        f"resume request did not use the expected Range header: {records[1]}",
    )
    require(
        records[1].if_range_header == ETAG,
        f"resume request did not use the expected If-Range header: {records[1]}",
    )
    require(
        "resumed" in result.stdout,
        "resume output did not include resumed transfer details",
    )


def print_failure_context(
    error: BaseException,
    results: list[CommandResult],
    records: list[RequestRecord],
    work_dir: pathlib.Path,
) -> None:
    print(
        f"Integration test failed: {type(error).__name__}: {error}",
        file=sys.stderr,
    )
    print(f"Work directory: {work_dir}", file=sys.stderr)
    print("--- traceback ---", file=sys.stderr)
    traceback.print_exception(type(error), error, error.__traceback__, file=sys.stderr)
    print("Recorded HTTP traffic:", file=sys.stderr)
    if records:
        for record in records:
            print(
                f"  {record.method} {record.path} "
                f"Range={record.range_header!r} If-Range={record.if_range_header!r}",
                file=sys.stderr,
            )
    else:
        print("  <no requests recorded>", file=sys.stderr)

    for result in results:
        print(f"\nCommand: {result.name}", file=sys.stderr)
        print("argv:", " ".join(result.argv), file=sys.stderr)
        print(f"exit code: {result.returncode}", file=sys.stderr)
        print("--- stdout ---", file=sys.stderr)
        print(result.stdout or "<empty>", file=sys.stderr)
        print("--- stderr ---", file=sys.stderr)
        print(result.stderr or "<empty>", file=sys.stderr)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oceandl", required=True, type=pathlib.Path)
    parser.add_argument("--work-dir", required=True, type=pathlib.Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    oceandl_path = args.oceandl.resolve()
    work_dir = args.work_dir.resolve()

    require(oceandl_path.exists(), f"oceandl binary was not found: {oceandl_path}")
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    config_path = work_dir / "config.toml"
    request_log = RequestLog()
    command_results: list[CommandResult] = []
    last_records: list[RequestRecord] = []

    try:
        with running_server(request_log) as server:
            port = server.server_address[1]
            write_config(config_path, port)

            download_output = work_dir / "download-output"
            request_log.clear()
            first_download = run_command("first-download", oceandl_path, config_path, download_output)
            command_results.append(first_download)
            last_records = request_log.snapshot()
            require_command_success(first_download)
            assert_first_download(first_download, last_records, download_output)

            request_log.clear()
            skip_rerun = run_command("skip-rerun", oceandl_path, config_path, download_output)
            command_results.append(skip_rerun)
            last_records = request_log.snapshot()
            require_command_success(skip_rerun)
            assert_skip_rerun(skip_rerun, last_records, download_output)

            resume_output = work_dir / "resume-output"
            write_resume_fixture(resume_output)
            request_log.clear()
            resume_download = run_command("resume-download", oceandl_path, config_path, resume_output)
            command_results.append(resume_download)
            last_records = request_log.snapshot()
            require_command_success(resume_download)
            assert_resume_download(resume_download, last_records, resume_output)

        print("Hermetic HTTP integration test passed.")
        return 0
    except BaseException as error:
        print_failure_context(error, command_results, last_records, work_dir)
        return 1


if __name__ == "__main__":
    sys.exit(main())
