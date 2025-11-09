# GitHub Actions CI Setup

This document describes how to set up continuous integration for the sparkle-duck project using GitHub Actions with a self-hosted runner.

## Overview

The project uses a self-hosted runner for CI builds because:
- Build dependencies (LVGL, libdatachannel, etc.) are large and cached locally.
- Physics simulations may require specific hardware or longer timeouts.
- Self-hosted runners are free and run on your own hardware.

## Workflow

The `.github/workflows/pr-check.yml` workflow runs on:
- Pull requests to `main`.
- Pushes to `main`.
- Manual dispatch (via GitHub UI).

### Build Steps

1. **Checkout** - Clones repository with submodules.
2. **Install dependencies** - Installs build tools (cmake, boost, etc.).
3. **Build debug** - Compiles debug build with symbols.
4. **Run tests** - Executes non-visual unit tests.
5. **Check formatting** - Verifies code follows clang-format style.
6. **Build release** - Compiles optimized release build.

## Self-Hosted Runner Setup

### Prerequisites

- Ubuntu/Debian-based Linux system.
- sudo access.
- GitHub account with repo access.

### Installation Steps

#### 1. Generate GitHub Token

Go to [GitHub Settings > Tokens](https://github.com/settings/tokens) and create a new token with `repo` scope (for private repos) or `public_repo` (for public repos).

#### 2. Configure Environment

From your build server:

```bash
# Clone the repository.
git clone https://github.com/YOUR_USERNAME/sparkle-duck.git
cd sparkle-duck/test-lvgl

# Copy the example environment file.
cp .github/.env.example .github/.env

# Edit .env and add your GitHub token.
nano .github/.env  # or use your preferred editor
```

Edit the `.github/.env` file and set your GitHub token:
```bash
GITHUB_TOKEN=ghp_your_actual_token_here
```

The other values (REPO_OWNER, REPO_NAME) will be auto-detected from your git remote, but you can override them if needed.

#### 3. Run Setup Script

```bash
# Run the setup script (reads from .env).
./.github/scripts/setup-runner.sh
```

The script will:
- Load configuration from `.env`.
- Install build dependencies.
- Download and configure the GitHub Actions runner.
- Register the runner with your repository.
- Install and start the runner as a system service.

#### 4. Verify Runner

Check that the runner is registered:

```bash
cd ~/actions-runner
sudo ./svc.sh status
```

You should also see the runner listed at:
`https://github.com/YOUR_USERNAME/sparkle-duck/settings/actions/runners`

### Manual Setup (Alternative)

If you prefer to set up the runner manually:

1. Go to your repository settings: `Settings > Actions > Runners > New self-hosted runner`.
2. Follow GitHub's instructions for Linux.
3. Install dependencies:
   ```bash
   sudo apt-get install -y build-essential cmake pkg-config libboost-dev clang-format
   ```
4. Configure and start the runner.

## Runner Management

### Check Status

```bash
cd ~/actions-runner
sudo ./svc.sh status
```

### Stop Runner

```bash
sudo ./svc.sh stop
```

### Start Runner

```bash
sudo ./svc.sh start
```

### Uninstall Runner

```bash
cd ~/actions-runner
sudo ./svc.sh stop
sudo ./svc.sh uninstall
./config.sh remove --token YOUR_REMOVAL_TOKEN
```

Get a removal token from:
`https://github.com/YOUR_USERNAME/sparkle-duck/settings/actions/runners`

## Configuration

### Runner Labels

The runner is registered with labels: `self-hosted`, `linux`, `x64`.

The workflow specifies `runs-on: self-hosted` to use this runner.

### Environment Variables

You can customize the runner by editing the `.github/.env` file:

```bash
# .github/.env
GITHUB_TOKEN=ghp_your_token_here
RUNNER_NAME=my-custom-name
RUNNER_LABELS=self-hosted,linux,x64,fast-build
RUNNER_WORK_DIR=_work
```

Available configuration options (all optional except GITHUB_TOKEN):
- `GITHUB_TOKEN`: Your GitHub personal access token (required).
- `REPO_OWNER`: Repository owner (auto-detected if not set).
- `REPO_NAME`: Repository name (auto-detected if not set).
- `RUNNER_NAME`: Name for this runner (default: `sparkle-duck-runner-<hostname>`).
- `RUNNER_LABELS`: Comma-separated list of labels (default: `self-hosted,linux,x64`).
- `RUNNER_WORK_DIR`: Working directory for jobs (default: `_work`).

### Workflow Customization

Edit `.github/workflows/pr-check.yml` to:
- Add more build configurations (ASAN, different compilers).
- Run visual tests (if display available).
- Add deployment steps.
- Configure notifications.

## Troubleshooting

### Runner Not Appearing

- Check that the runner service is running: `sudo ./svc.sh status`.
- Check logs: `journalctl -u actions.runner.*`.
- Verify the registration token hasn't expired (they expire after 1 hour).

### Build Failures

- Check runner logs in `~/actions-runner/_diag/`.
- Ensure all dependencies are installed.
- Test the build manually: `cd ~/actions-runner/_work/sparkle-duck/sparkle-duck && make debug`.

### Permission Issues

- Ensure the runner user has sudo access (needed for `apt-get install`).
- Or pre-install all dependencies and remove the install step from the workflow.

## Security Considerations

### Runner Access

Self-hosted runners have access to:
- Your local network.
- The user account they run under.
- Any credentials in that user's environment.

**For public repositories:** Only use self-hosted runners if you trust all potential contributors, as they can run arbitrary code on your runner via pull requests.

**Recommendation:** For private repos only, or use GitHub's hosted runners for public repos.

### Token Security

- Store GitHub tokens securely (use a password manager).
- Tokens with `repo` scope have full repository access.
- Rotate tokens periodically.
- Never commit tokens to the repository.

## Advanced Usage

### Multiple Runners

To run multiple runners (e.g., on different machines), create separate `.env` files with different configurations:

```bash
# On machine 1 (fast build server).
cp .github/.env.example .github/.env
# Edit .github/.env:
#   RUNNER_NAME=sparkle-duck-fast
#   RUNNER_LABELS=self-hosted,linux,x64,fast-build
./.github/scripts/setup-runner.sh

# On machine 2 (slower build server).
cp .github/.env.example .github/.env
# Edit .github/.env:
#   RUNNER_NAME=sparkle-duck-slow
#   RUNNER_LABELS=self-hosted,linux,x64,slow-build
./.github/scripts/setup-runner.sh
```

Then use labels to target specific runners:

```yaml
jobs:
  fast-build:
    runs-on: [self-hosted, fast-build]

  slow-build:
    runs-on: [self-hosted, slow-build]
```

### Runner Updates

GitHub Actions runner auto-updates, but you can manually update:

```bash
cd ~/actions-runner
sudo ./svc.sh stop
./config.sh remove --token REMOVAL_TOKEN
# Re-run .github/scripts/setup-runner.sh with new version
```

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Self-hosted Runners Guide](https://docs.github.com/en/actions/hosting-your-own-runners)
- [Runner Application](https://github.com/actions/runner)
