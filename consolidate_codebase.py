#!/usr/bin/env python3
"""
Codebase Consolidation Script for LLM Analysis

This script consolidates all code files in the open-reactor-control-system project into a single
text file that can be passed to an LLM for analysis and questioning.

Features:
- Filters out binary files, cache files, and build artifacts
- Includes comprehensive file headers for context
- Respects .gitignore patterns
- Handles encoding detection and error recovery
- Provides statistics and progress reporting
- Configurable file types and size limits
- Intelligently excludes common non-source directories
- Token counting for context window management
- Project-specific file prioritization
"""

import os
import sys
import argparse
import fnmatch
from pathlib import Path
from typing import List, Set, Optional, Dict, Any, Tuple
import chardet
import mimetypes
from datetime import datetime
import re


class CodebaseConsolidator:
    """
    Consolidates codebase files into a single text file for LLM analysis.

    This class provides intelligent filtering, encoding detection, and
    comprehensive file organization for optimal LLM consumption.
    """

    def __init__(
        self, project_root: str, output_file: str = "consolidated_codebase.txt"
    ):
        """
        Initialize the consolidator.

        Args:
            project_root (str): Path to the project root directory
            output_file (str): Output file name/path
        """
        self.project_root = Path(project_root).resolve()
        self.output_file = Path(output_file)

        # Configuration
        self.max_file_size = 1024 * 1024  # 1MB limit per file
        self.max_total_size = 50 * 1024 * 1024  # 50MB total limit
        self.max_tokens = 1000000  # Approximate token limit for large context models

        # File type filtering - Enhanced with more types
        self.source_extensions = {
            # Python
            ".py",
            ".pyx",
            ".pyi",
            ".pth",
            # JavaScript/TypeScript
            ".js",
            ".jsx",
            ".ts",
            ".tsx",
            ".mjs",
            ".cjs",
            # Web technologies
            ".html",
            ".htm",
            ".css",
            ".scss",
            ".sass",
            ".less",
            ".vue",
            ".svelte",
            # Data/Config formats
            ".sql",
            ".yaml",
            ".yml",
            ".json",
            ".toml",
            ".xml",
            ".xsd",
            ".xsl",
            # Documentation
            ".md",
            ".rst",
            ".txt",
            ".adoc",
            ".tex",
            # Shell scripts
            ".sh",
            ".bash",
            ".zsh",
            ".fish",
            ".ps1",
            ".bat",
            ".cmd",
            # Docker and containers
            ".dockerfile",
            ".containerfile",
            # Configuration files
            ".env",
            ".gitignore",
            ".gitattributes",
            ".editorconfig",
            ".cfg",
            ".conf",
            ".ini",
            ".properties",
            # Infrastructure as Code
            ".tf",
            ".tfvars",
            ".hcl",
            ".nomad",
            ".pkr.hcl",
            # CI/CD
            ".github",
            ".gitlab-ci.yml",
            ".travis.yml",
            ".circleci",
            # Programming languages
            ".go",
            ".rs",
            ".cpp",
            ".c",
            ".h",
            ".hpp",
            ".cc",
            ".cxx",
            ".java",
            ".kt",
            ".scala",
            ".clj",
            ".cljs",
            ".swift",
            ".rb",
            ".php",
            ".perl",
            ".pl",
            ".r",
            ".R",
            ".lua",
            ".dart",
            ".elm",
            ".ex",
            ".exs",
            ".fs",
            ".fsx",
            ".ino", # Arduino sketch files
            # Data science
            ".ipynb",
            ".py",
            ".r",
            ".R",
            ".sql",
            ".jl",
            # Cloud/Kubernetes
            ".k8s.yaml",
            ".k8s.yml",
            ".helm",
            ".kustomization.yaml",
        }

        # Priority files that should be included first (most important for context)
        self.priority_files = {
            "README.md",
            "README.rst",
            "README.txt",
            "README",
            "PLANNING.md",
            "TASK.md",
            "TASKS.md",
            "platformio.ini", # Added for PlatformIO
            "pyproject.toml",
            "setup.py",
            "requirements.txt",
            "Pipfile",
            "package.json",
            "package-lock.json",
            "yarn.lock",
            "Dockerfile",
            "docker-compose.yml",
            "docker-compose.yaml",
            "Makefile",
            "makefile",
            ".env.example",
            "LICENSE",
            "LICENCE",
            "CHANGELOG.md",
            "CHANGELOG",
            "CONTRIBUTING.md",
            "CODE_OF_CONDUCT.md",
            "__init__.py",
            "main.py",
            "main.cpp", # Added for C/C++
            "app.py",
            "server.py",
            "config.py",
            "settings.py",
            "constants.py",
        }

        # Directories to exclude - Enhanced list
        self.excluded_dirs = {
            # Python
            "__pycache__",
            ".pytest_cache",
            ".mypy_cache",
            ".tox",
            "venv",
            ".venv",
            "env",
            ".env",
            "virtualenv",
            "site-packages",
            # PlatformIO
            ".pio",
            ".platformio",
            # Node.js
            "node_modules",
            ".npm",
            ".yarn",
            ".pnp",
            # Build artifacts
            "build",
            "dist",
            ".build",
            ".dist",
            "target",
            "bin",
            "obj",
            "out",
            "output",
            ".next",
            ".nuxt",
            ".svelte-kit",
            # IDEs
            ".idea",
            ".vscode",
            ".vs",
            ".atom",
            ".sublime-project",
            # Version control
            ".git",
            ".svn",
            ".hg",
            ".bzr",
            # Logs and temporary files
            "logs",
            "log",
            "tmp",
            "temp",
            ".tmp",
            "cache",
            ".cache",
            # Coverage and testing
            ".coverage",
            "htmlcov",
            ".nyc_output",
            "coverage",
            # Documentation builds
            "_build",
            "docs/_build",
            "site",
            ".docusaurus",
            # Database
            "migrations",
            "alembic",
            "db_data",
            "data",
            # Cloud/deployment
            ".terraform",
            ".pulumi",
            ".serverless",
            # OS specific
            ".DS_Store",
            "Thumbs.db",
        }

        # File patterns to exclude - Enhanced patterns
        self.excluded_patterns = {
            # Compiled files
            "*.pyc",
            "*.pyo",
            "*.pyd",
            "*.so",
            "*.dll",
            "*.dylib",
            "*.o",
            "*.obj",
            "*.lib",
            "*.a",
            "*.class",
            "*.elf",  # Added for embedded
            "*.bin",  # Added for embedded
            "*.hex",  # Added for embedded
            # Executables
            "*.exe",
            "*.msi",
            "*.dmg",
            "*.pkg",
            "*.deb",
            "*.rpm",
            # Archives
            "*.zip",
            "*.tar",
            "*.gz",
            "*.bz2",
            "*.7z",
            "*.rar",
            # Media files
            "*.jpg",
            "*.jpeg",
            "*.png",
            "*.gif",
            "*.bmp",
            "*.svg",
            "*.ico",
            "*.mp3",
            "*.mp4",
            "*.avi",
            "*.mov",
            "*.wav",
            "*.wmv",
            # Documents
            "*.pdf",
            "*.doc",
            "*.docx",
            "*.xls",
            "*.xlsx",
            "*.ppt",
            "*.pptx",
            # Lock and log files
            "*.lock",
            "*.log",
            "*.out",
            "*.err",
            "*.tmp",
            # Minified files
            "*.min.js",
            "*.min.css",
            "*.bundle.js",
            "*.chunk.js",
            # Database files
            "*.db",
            "*.sqlite",
            "*.sqlite3",
            "*.mdb",
            # Backup files
            "*.bak",
            "*.backup",
            "*.old",
            "*~",
            "*.swp",
            "*.swo",
            # OS files
            ".DS_Store",
            "Thumbs.db",
            "desktop.ini",
        }

        # Gitignore patterns
        self.gitignore_patterns = []
        self._load_gitignore()

        # Statistics
        self.stats = {
            "total_files_scanned": 0,
            "files_included": 0,
            "files_excluded": 0,
            "total_size": 0,
            "estimated_tokens": 0,
            "errors": 0,
            "encoding_issues": 0,
            "priority_files_found": 0,
        }

    def _load_gitignore(self) -> None:
        """Load and parse .gitignore file if it exists."""
        gitignore_path = self.project_root / ".gitignore"
        if gitignore_path.exists():
            try:
                with open(gitignore_path, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith("#"):
                            self.gitignore_patterns.append(line)
            except Exception:
                pass  # Silently continue if gitignore can't be read

    def _matches_gitignore(self, file_path: Path) -> bool:
        """Check if file matches any gitignore pattern."""
        relative_path = str(file_path.relative_to(self.project_root))
        for pattern in self.gitignore_patterns:
            if fnmatch.fnmatch(relative_path, pattern) or fnmatch.fnmatch(
                file_path.name, pattern
            ):
                return True
        return False

    def _estimate_tokens(self, text: str) -> int:
        """
        Rough estimation of tokens in text.
        Uses simple heuristic: ~4 characters per token for code.
        """
        return len(text) // 4

    def _is_binary_file(self, file_path: Path) -> bool:
        """
        Check if a file is binary using multiple methods.

        Args:
            file_path (Path): Path to the file to check

        Returns:
            bool: True if file appears to be binary
        """
        # Check MIME type
        mime_type, _ = mimetypes.guess_type(str(file_path))
        if (
            mime_type
            and not mime_type.startswith("text/")
            and mime_type != "application/json"
        ):
            return True

        # Check file content (first 8192 bytes)
        try:
            with open(file_path, "rb") as f:
                chunk = f.read(8192)
                if b"\0" in chunk:  # Null bytes indicate binary
                    return True

                # Check for high percentage of non-printable characters
                printable_chars = sum(
                    1 for byte in chunk if 32 <= byte <= 126 or byte in (9, 10, 13)
                )
                if len(chunk) > 0 and printable_chars / len(chunk) < 0.7:
                    return True

        except (IOError, OSError):
            return True

        return False

    def _should_exclude_file(self, file_path: Path) -> Tuple[bool, str]:
        """
        Determine if a file should be excluded from consolidation.

        Args:
            file_path (Path): Path to the file to check

        Returns:
            Tuple[bool, str]: (should_exclude, reason)
        """
        # Check gitignore patterns first
        if self._matches_gitignore(file_path):
            return True, "Matches .gitignore pattern"

        # Check file size
        try:
            file_size = file_path.stat().st_size
            if file_size > self.max_file_size:
                return True, f"File too large ({file_size:,} bytes)"
            if file_size == 0:
                return True, "Empty file"
        except OSError:
            return True, "Cannot access file"

        # Check extension and special files
        file_name = file_path.name.lower()
        file_ext = file_path.suffix.lower()

        is_special_file = (
            file_name in {f.lower() for f in self.priority_files}
            or file_name.startswith((".env", "dockerfile", "makefile"))
            or file_ext in self.source_extensions
        )

        if not is_special_file:
            return True, f"Extension {file_ext} not in source extensions"

        # Check excluded patterns
        for pattern in self.excluded_patterns:
            if fnmatch.fnmatch(file_name, pattern):
                return True, f"Matches excluded pattern: {pattern}"

        # Check if binary
        if self._is_binary_file(file_path):
            return True, "Binary file detected"

        return False, ""

    def _should_exclude_directory(self, dir_path: Path) -> Tuple[bool, str]:
        """
        Determine if a directory should be excluded.

        Args:
            dir_path (Path): Path to the directory to check

        Returns:
            Tuple[bool, str]: (should_exclude, reason)
        """
        dir_name = dir_path.name.lower()

        if dir_name in self.excluded_dirs:
            return True, f"Directory {dir_name} in excluded list"

        # Check gitignore patterns
        if self._matches_gitignore(dir_path):
            return True, "Matches .gitignore pattern"

        # Check for hidden directories (except important ones)
        if dir_name.startswith(".") and dir_name not in {
            ".github",
            ".vscode",
            ".devcontainer",
            ".docker",
        }:
            return True, f"Hidden directory: {dir_name}"

        return False, ""

    def _detect_encoding(self, file_path: Path) -> str:
        """
        Detect file encoding using chardet.

        Args:
            file_path (Path): Path to the file

        Returns:
            str: Detected encoding or 'utf-8' as fallback
        """
        try:
            with open(file_path, "rb") as f:
                raw_data = f.read(min(10000, file_path.stat().st_size))
                result = chardet.detect(raw_data)
                return result.get("encoding", "utf-8") or "utf-8"
        except Exception:
            return "utf-8"

    def _read_file_content(self, file_path: Path) -> Optional[str]:
        """
        Read file content with encoding detection and error handling.

        Args:
            file_path (Path): Path to the file to read

        Returns:
            Optional[str]: File content or None if unable to read
        """
        encodings_to_try = [
            self._detect_encoding(file_path),
            "utf-8",
            "latin-1",
            "cp1252",
            "iso-8859-1",
        ]

        for encoding in encodings_to_try:
            try:
                with open(file_path, "r", encoding=encoding, errors="replace") as f:
                    content = f.read()
                    # Verify content is reasonable
                    if len(content.encode("utf-8", errors="ignore")) > 0:
                        return content
            except (UnicodeDecodeError, UnicodeError, OSError):
                continue

        self.stats["encoding_issues"] += 1
        return None

    def _get_file_list(self) -> List[Path]:
        """
        Get prioritized list of all files to potentially include.

        Returns:
            List[Path]: List of file paths to process, with priority files first
        """
        all_files = []
        priority_files = []

        for root, dirs, filenames in os.walk(self.project_root):
            root_path = Path(root)

            # Filter directories in-place to avoid walking into excluded ones
            dirs[:] = [
                d for d in dirs if not self._should_exclude_directory(root_path / d)[0]
            ]

            for filename in filenames:
                file_path = root_path / filename

                # Separate priority files
                if filename.lower() in {f.lower() for f in self.priority_files}:
                    priority_files.append(file_path)
                else:
                    all_files.append(file_path)

        # Return priority files first, then others
        return sorted(priority_files) + sorted(all_files)

    def _write_file_section(self, output_file, file_path: Path, content: str) -> None:
        """
        Write a file section to the output file.

        Args:
            output_file: Open file handle for output
            file_path (Path): Path to the source file
            content (str): File content to write
        """
        relative_path = file_path.relative_to(self.project_root)
        file_size = len(content.encode("utf-8"))
        estimated_tokens = self._estimate_tokens(content)

        # File header
        output_file.write("=" * 80 + "\n")
        output_file.write(f"FILE: {relative_path}\n")
        output_file.write(f"SIZE: {file_size:,} bytes\n")
        output_file.write(f"TOKENS: ~{estimated_tokens:,}\n")
        output_file.write(f"TYPE: {file_path.suffix or 'no extension'}\n")

        # Mark priority files
        if file_path.name in self.priority_files:
            output_file.write("PRIORITY: HIGH (Project documentation/config)\n")

        output_file.write("=" * 80 + "\n\n")

        # File content
        output_file.write(content)

        # Ensure content ends with newline
        if not content.endswith("\n"):
            output_file.write("\n")

        output_file.write("\n\n")

        # Update token count
        self.stats["estimated_tokens"] += estimated_tokens

    def consolidate(
        self, include_stats: bool = True, verbose: bool = False
    ) -> Dict[str, Any]:
        """
        Consolidate the codebase into a single file.

        Args:
            include_stats (bool): Whether to include statistics in output
            verbose (bool): Whether to print progress information

        Returns:
            Dict[str, Any]: Consolidation statistics and results
        """
        if verbose:
            print(f"🔍 Scanning codebase in: {self.project_root}")
            print(f"📝 Output file: {self.output_file}")

        files = self._get_file_list()
        self.stats["total_files_scanned"] = len(files)

        if verbose:
            print(f"📁 Found {len(files)} files to examine")

        # Open output file
        try:
            with open(self.output_file, "w", encoding="utf-8") as output:
                # Write header
                output.write("OPEN REACTOR CONTROL SYSTEM CODEBASE CONSOLIDATION\n")
                output.write("=" * 80 + "\n")
                output.write(f"Generated: {datetime.now().isoformat()}\n")
                output.write(f"Project Root: {self.project_root}\n")
                output.write(f"Total Files Scanned: {len(files)}\n")
                output.write(f"Max Tokens Target: {self.max_tokens:,}\n")
                output.write("=" * 80 + "\n")
                output.write("\nIMPORTANT: This consolidation prioritizes:\n")
                output.write("1. Documentation and configuration files\n")
                output.write("2. Core application code\n")
                output.write("3. Tests and supporting files\n")
                output.write(
                    "4. Files are ordered by importance for project understanding\n"
                )
                output.write("=" * 80 + "\n\n")

                # Process each file
                for i, file_path in enumerate(files):
                    if verbose and i % 50 == 0:
                        print(
                            f"⏳ Processing file {i+1}/{len(files)}: {file_path.name}"
                        )

                    # Check if we should exclude this file
                    should_exclude, reason = self._should_exclude_file(file_path)
                    if should_exclude:
                        self.stats["files_excluded"] += 1
                        if verbose and "debug" in sys.argv:
                            print(f"   ⏭️  Skipping: {reason}")
                        continue

                    # Check size and token limits
                    if (
                        self.stats["total_size"] > self.max_total_size
                        or self.stats["estimated_tokens"] > self.max_tokens
                    ):
                        if verbose:
                            print(
                                f"⚠️  Reached limits (Size: {self.stats['total_size']:,}, Tokens: {self.stats['estimated_tokens']:,})"
                            )
                        break

                    # Read file content
                    try:
                        content = self._read_file_content(file_path)
                        if content is None:
                            self.stats["errors"] += 1
                            if verbose:
                                print(f"   ❌ Could not read: {file_path}")
                            continue

                        # Write to output
                        self._write_file_section(output, file_path, content)

                        self.stats["files_included"] += 1
                        self.stats["total_size"] += len(content.encode("utf-8"))

                        # Track priority files
                        if file_path.name in self.priority_files:
                            self.stats["priority_files_found"] += 1

                    except Exception as e:
                        self.stats["errors"] += 1
                        if verbose:
                            print(f"   ❌ Error processing {file_path}: {e}")
                        continue

                # Write statistics if requested
                if include_stats:
                    output.write("\n" + "=" * 80 + "\n")
                    output.write("CONSOLIDATION STATISTICS\n")
                    output.write("=" * 80 + "\n")
                    for key, value in self.stats.items():
                        output.write(f"{key.replace('_', ' ').title()}: {value:,}\n")

                    # Context window usage
                    context_usage = (
                        self.stats["estimated_tokens"] / self.max_tokens
                    ) * 100
                    output.write(f"Context Window Usage: {context_usage:.1f}%\n")

        except IOError as e:
            raise RuntimeError(
                f"Could not write to output file {self.output_file}: {e}"
            )

        if verbose:
            print("\n✅ Consolidation complete!")
            print(f"📊 Files included: {self.stats['files_included']:,}")
            print(f"📊 Files excluded: {self.stats['files_excluded']:,}")
            print(f"📊 Priority files: {self.stats['priority_files_found']:,}")
            print(f"📊 Total size: {self.stats['total_size']:,} bytes")
            print(f"📊 Estimated tokens: {self.stats['estimated_tokens']:,}")
            print(f"📊 Errors: {self.stats['errors']:,}")

        return self.stats


def main():
    """Main entry point for the script."""
    parser = argparse.ArgumentParser(
        description="Consolidate codebase files into a single text file for LLM analysis",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python consolidate_codebase.py
  python consolidate_codebase.py --output my_codebase.txt
  python consolidate_codebase.py --project-root /path/to/project --verbose
  python consolidate_codebase.py --no-stats --max-tokens 500000
        """,
    )

    parser.add_argument(
        "--project-root",
        default=".",
        help="Path to the project root directory (default: current directory)",
    )

    parser.add_argument(
        "--output",
        "-o",
        default="consolidated_codebase.txt",
        help="Output file path (default: consolidated_codebase.txt)",
    )

    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose output"
    )

    parser.add_argument(
        "--no-stats",
        action="store_true",
        help="Do not include statistics in the output file",
    )

    parser.add_argument(
        "--max-tokens",
        type=int,
        default=1000000,
        help="Maximum estimated tokens to include (default: 1000000)",
    )

    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug output showing excluded files",
    )

    args = parser.parse_args()

    try:
        consolidator = CodebaseConsolidator(
            project_root=args.project_root, output_file=args.output
        )

        # Override max tokens if specified
        if args.max_tokens:
            consolidator.max_tokens = args.max_tokens

        stats = consolidator.consolidate(
            include_stats=not args.no_stats, verbose=args.verbose
        )

        print(f"\n🎉 Successfully consolidated codebase!")
        print(f"📄 Output file: {consolidator.output_file}")
        print(
            f"📈 Files processed: {stats['files_included']:,} included, {stats['files_excluded']:,} excluded"
        )
        print(f"🎯 Priority files found: {stats['priority_files_found']:,}")
        print(f"📊 Estimated tokens: {stats['estimated_tokens']:,}")

        # Context window warnings
        context_usage = (stats["estimated_tokens"] / consolidator.max_tokens) * 100
        if context_usage > 90:
            print(
                f"⚠️  High context usage: {context_usage:.1f}% - consider reducing scope"
            )
        elif context_usage > 70:
            print(f"📋 Context usage: {context_usage:.1f}% - good utilization")
        else:
            print(f"✅ Context usage: {context_usage:.1f}% - room for more content")

        if stats["errors"] > 0:
            print(f"⚠️  Warnings: {stats['errors']} files had errors")

        return 0

    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
