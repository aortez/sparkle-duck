#!/usr/bin/env node
/**
 * Sparkle Duck deployment script
 * Syncs code to remote Pi and optionally builds/restarts the service
 */

import { spawn } from 'child_process';
import { parseArgs } from 'util';

const { values: args, positionals } = parseArgs({
  options: {
    host: { type: 'string', short: 'h', description: 'SSH host (user@hostname)' },
    path: { type: 'string', short: 'p', description: 'Remote path to project' },
    'ssh-key': { type: 'string', short: 'i', description: 'Path to SSH private key' },
    build: { type: 'boolean', short: 'b', default: true, description: 'Run build after sync' },
    restart: { type: 'boolean', short: 'r', default: true, description: 'Restart service after build' },
    'build-type': { type: 'string', default: 'release', description: 'Build type (debug/release)' },
    debug: { type: 'boolean', short: 'd', default: false, description: 'Build debug instead of release' },
    'dry-run': { type: 'boolean', short: 'n', default: false, description: 'Show what would be done' },
    local: { type: 'boolean', short: 'l', default: false, description: 'Local deploy (skip rsync, run locally)' },
    help: { type: 'boolean', default: false },
  },
  allowPositionals: true,
});

function usage() {
  console.log(`
Sparkle Duck Deploy Script

Usage: deploy.mjs [options]

Options:
  -h, --host <user@host>   SSH host (required for remote deploy)
  -p, --path <path>        Remote project path (required for remote deploy)
  -i, --ssh-key <path>     SSH private key path
  -b, --build              Run build after sync (default: true)
  -r, --restart            Restart service after build (default: true)
  --build-type <type>      Build type: debug or release (default: release)
  -d, --debug              Build debug instead of release (shortcut for --build-type debug)
  -n, --dry-run            Show commands without executing
  -l, --local              Local deploy (skip rsync, build and restart locally)
  --help                   Show this help

Examples:
  # Remote deploy (release build, default)
  ./deploy.mjs -h oldman@pi5 -p /home/oldman/workspace/sparkle-duck/test-lvgl

  # Remote deploy with debug build
  ./deploy.mjs -d -h oldman@pi5 -p /home/oldman/workspace/sparkle-duck/test-lvgl

  # Local deploy (on Pi itself)
  ./deploy.mjs --local
`);
  process.exit(0);
}

if (args.help) usage();

if (!args.local && (!args.host || !args.path)) {
  console.error('Error: --host and --path are required for remote deploy (or use --local)');
  usage();
}

function run(cmd, cmdArgs, options = {}) {
  return new Promise((resolve, reject) => {
    const fullCmd = `${cmd} ${cmdArgs.join(' ')}`;

    if (args['dry-run']) {
      console.log(`[dry-run] ${fullCmd}`);
      resolve({ code: 0, stdout: '', stderr: '' });
      return;
    }

    console.log(`\n> ${fullCmd}`);

    const proc = spawn(cmd, cmdArgs, {
      stdio: 'inherit',
      ...options,
    });

    proc.on('close', (code) => {
      if (code === 0) {
        resolve({ code });
      } else {
        reject(new Error(`Command failed with code ${code}: ${fullCmd}`));
      }
    });

    proc.on('error', (err) => {
      reject(new Error(`Failed to execute: ${fullCmd}\n${err.message}`));
    });
  });
}

function sshArgs() {
  const sshOpts = ['-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=accept-new'];
  if (args['ssh-key']) {
    sshOpts.push('-i', args['ssh-key']);
  }
  return sshOpts;
}

// Try to run a command, but don't fail if it errors (used for pause/resume).
async function tryRun(cmd, cmdArgs, options = {}) {
  try {
    await run(cmd, cmdArgs, options);
    return true;
  } catch (err) {
    console.log(`  (command failed, continuing: ${err.message})`);
    return false;
  }
}

async function main() {
  const { host, path, local } = args;
  // --debug flag overrides --build-type.
  const buildType = args.debug ? 'debug' : args['build-type'];

  console.log('=== Sparkle Duck Deploy ===');
  if (local) {
    console.log('Mode: local');
  } else {
    console.log(`Target: ${host}:${path}`);
  }
  console.log(`Build: ${args.build ? buildType : 'skip'}`);
  console.log(`Restart: ${args.restart ? 'yes' : 'no'}`);

  if (local) {
    // Local deployment - no rsync needed.
    const uiAddress = 'ws://localhost:7070';
    const cliPath = './build-debug/bin/cli';

    // Pause simulation before build (if running).
    console.log('\n--- Pausing simulation ---');
    const paused = await tryRun(cliPath, ['sim_pause', uiAddress]);

    if (args.build) {
      console.log('\n--- Building ---');
      await run('make', [buildType]);
    }

    if (args.restart) {
      console.log('\n--- Restarting service ---');
      await run('systemctl', ['--user', 'restart', 'sparkle-duck.service']);
      await run('sleep', ['1']);
      await run('systemctl', ['--user', 'status', 'sparkle-duck.service', '--no-pager']);
    }

    // Resume simulation if we paused it.
    if (paused) {
      console.log('\n--- Resuming simulation ---');
      await tryRun(cliPath, ['sim_run', uiAddress]);
    }
  } else {
    // Remote deployment
    // Extract hostname from user@host format for WebSocket address.
    const hostname = host.includes('@') ? host.split('@')[1] : host;
    const uiAddress = `ws://${hostname}:7070`;

    // Pause simulation before deploy (if running).
    console.log('\n--- Pausing simulation ---');
    const cliPath = './build-debug/bin/cli';
    const paused = await tryRun(cliPath, ['sim_pause', uiAddress]);

    // Step 1: Sync files with rsync.
    console.log('\n--- Syncing files ---');
    const rsyncArgs = [
      '-avz', '--delete',
      '--exclude', 'build-*',
      '--exclude', '.git',
      '--exclude', 'node_modules',
      '--exclude', '*.log',
      '--exclude', '.cache',
      '-e', `ssh ${sshArgs().join(' ')}`,
      './',
      `${host}:${path}/`,
    ];
    await run('rsync', rsyncArgs);

    // Step 2: Build on remote (if requested).
    if (args.build) {
      console.log('\n--- Building ---');
      const buildCmd = `cd ${path} && make ${buildType}`;
      await run('ssh', [...sshArgs(), host, buildCmd]);
    }

    // Step 3: Restart service (if requested).
    if (args.restart) {
      console.log('\n--- Restarting service ---');
      const restartCmd = `systemctl --user restart sparkle-duck.service && sleep 1 && systemctl --user status sparkle-duck.service --no-pager`;
      await run('ssh', [...sshArgs(), host, restartCmd]);
    }

    // Resume simulation if we paused it.
    if (paused) {
      console.log('\n--- Resuming simulation ---');
      await tryRun(cliPath, ['sim_run', uiAddress]);
    }
  }

  console.log('\n=== Deploy complete ===');
}

main().catch((err) => {
  console.error(`\nDeploy failed: ${err.message}`);
  process.exit(1);
});
