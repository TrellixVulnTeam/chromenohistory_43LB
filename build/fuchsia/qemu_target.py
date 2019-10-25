# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running and interacting with Fuchsia on QEMU."""

import boot_data
import common
import emu_target
import logging
import md5
import os
import platform
import shutil
import subprocess
import sys
import tempfile

from common import GetEmuRootForPlatform, EnsurePathExists


# Virtual networking configuration data for QEMU.
GUEST_NET = '192.168.3.0/24'
GUEST_IP_ADDRESS = '192.168.3.9'
HOST_IP_ADDRESS = '192.168.3.2'
GUEST_MAC_ADDRESS = '52:54:00:63:5e:7b'

# Capacity of the system's blobstore volume.
EXTENDED_BLOBSTORE_SIZE = 1073741824  # 1GB


class QemuTarget(emu_target.EmuTarget):
  def __init__(self, output_dir, target_cpu, system_log_file,
               emu_type, cpu_cores, require_kvm, ram_size_mb):
    super(QemuTarget, self).__init__(output_dir, target_cpu,
                                     system_log_file)
    self._emu_type=emu_type
    self._cpu_cores=cpu_cores
    self._require_kvm=require_kvm
    self._ram_size_mb=ram_size_mb

  def _GetEmulatorName(self):
    return self._emu_type

  def _IsKvmEnabled(self):
    if self._require_kvm:
      if (sys.platform.startswith('linux') and
          os.access('/dev/kvm', os.R_OK | os.W_OK)):
        if self._target_cpu == 'arm64' and platform.machine() == 'aarch64':
          return True
        if self._target_cpu == 'x64' and platform.machine() == 'x86_64':
          return True
    return False

  def _BuildQemuConfig(self):
    boot_data.AssertBootImagesExist(self._GetTargetSdkArch(), 'qemu')

    emu_command = [
        '-kernel', EnsurePathExists(
            boot_data.GetTargetFile('qemu-kernel.kernel',
                                    self._GetTargetSdkArch(),
                                    boot_data.TARGET_TYPE_QEMU)),
        '-initrd', EnsurePathExists(
            boot_data.GetBootImage(self._output_dir, self._GetTargetSdkArch(),
                                   boot_data.TARGET_TYPE_QEMU)),
        '-m', str(self._ram_size_mb),
        '-smp', str(self._cpu_cores),

        # Attach the blobstore and data volumes. Use snapshot mode to discard
        # any changes.
        '-snapshot',
        '-drive', 'file=%s,format=qcow2,if=none,id=blobstore,snapshot=on' %
                    _EnsureBlobstoreQcowAndReturnPath(self._output_dir,
                                                      self._GetTargetSdkArch()),
        '-device', 'virtio-blk-pci,drive=blobstore',

        # Use stdio for the guest OS only; don't attach the QEMU interactive
        # monitor.
        '-serial', 'stdio',
        '-monitor', 'none',
      ]

    # Configure the machine to emulate, based on the target architecture.
    if self._target_cpu == 'arm64':
      emu_command.extend([
          '-machine','virt',
      ])
      netdev_type = 'virtio-net-pci'
    else:
      emu_command.extend([
          '-machine', 'q35',
      ])
      netdev_type = 'e1000'

    # Configure virtual network. It is used in the tests to connect to
    # testserver running on the host.
    netdev_config = 'user,id=net0,net=%s,dhcpstart=%s,host=%s' % \
            (GUEST_NET, GUEST_IP_ADDRESS, HOST_IP_ADDRESS)

    self._host_ssh_port = common.GetAvailableTcpPort()
    netdev_config += ",hostfwd=tcp::%s-:22" % self._host_ssh_port
    emu_command.extend([
      '-netdev', netdev_config,
      '-device', '%s,netdev=net0,mac=%s' % (netdev_type, GUEST_MAC_ADDRESS),
    ])

    # Configure the CPU to emulate.
    # On Linux, we can enable lightweight virtualization (KVM) if the host and
    # guest architectures are the same.
    if self._IsKvmEnabled():
      kvm_command = ['-enable-kvm', '-cpu', 'host,migratable=no']
    else:
      logging.warning('Unable to launch %s with KVM acceleration.'
                       % (self._emu_type) +
                      'The guest VM will be slow.')
      if self._target_cpu == 'arm64':
        kvm_command = ['-cpu', 'cortex-a53']
      else:
        kvm_command = ['-cpu', 'Haswell,+smap,-check,-fsgsbase']

    emu_command.extend(kvm_command)

    kernel_args = boot_data.GetKernelArgs(self._output_dir)

    # TERM=dumb tells the guest OS to not emit ANSI commands that trigger
    # noisy ANSI spew from the user's terminal emulator.
    kernel_args.append('TERM=dumb')

    # Construct kernel cmd line
    kernel_args.append('kernel.serial=legacy')

    # Don't 'reboot' the emulator if the kernel crashes
    kernel_args.append('kernel.halt-on-panic=true')

    emu_command.extend(['-append', ' '.join(kernel_args)])

    return emu_command

  def _BuildCommand(self):
    qemu_exec = 'qemu-system-'+self._GetTargetSdkLegacyArch()
    qemu_command = [os.path.join(GetEmuRootForPlatform(self._emu_type), 'bin',
                                 qemu_exec)]
    qemu_command.extend(self._BuildQemuConfig())
    qemu_command.append('-nographic')
    return qemu_command

def _ComputeFileHash(filename):
  hasher = md5.new()
  with open(filename, 'rb') as f:
    buf = f.read(4096)
    while buf:
      hasher.update(buf)
      buf = f.read(4096)

  return hasher.hexdigest()


def _EnsureBlobstoreQcowAndReturnPath(output_dir, target_arch):
  """Returns a file containing the Fuchsia blobstore in a QCOW format,
  with extra buffer space added for growth."""

  qimg_tool = os.path.join(common.GetEmuRootForPlatform('qemu'),
                           'bin', 'qemu-img')
  fvm_tool = os.path.join(common.SDK_ROOT, 'tools', 'fvm')
  blobstore_path = boot_data.GetTargetFile('storage-full.blk', target_arch,
                                           'qemu')
  qcow_path = os.path.join(output_dir, 'gen', 'blobstore.qcow')

  # Check a hash of the blobstore to determine if we can re-use an existing
  # extended version of it.
  blobstore_hash_path = os.path.join(output_dir, 'gen', 'blobstore.hash')
  current_blobstore_hash = _ComputeFileHash(blobstore_path)

  if os.path.exists(blobstore_hash_path) and os.path.exists(qcow_path):
    if current_blobstore_hash == open(blobstore_hash_path, 'r').read():
      return qcow_path

  # Add some extra room for growth to the Blobstore volume.
  # Fuchsia is unable to automatically extend FVM volumes at runtime so the
  # volume enlargement must be performed prior to QEMU startup.

  # The 'fvm' tool only supports extending volumes in-place, so make a
  # temporary copy of 'blobstore.bin' before it's mutated.
  extended_blobstore = tempfile.NamedTemporaryFile()
  shutil.copyfile(blobstore_path, extended_blobstore.name)
  subprocess.check_call([fvm_tool, extended_blobstore.name, 'extend',
                         '--length', str(EXTENDED_BLOBSTORE_SIZE),
                         blobstore_path])

  # Construct a QCOW image from the extended, temporary FVM volume.
  # The result will be retained in the build output directory for re-use.
  subprocess.check_call([qimg_tool, 'convert', '-f', 'raw', '-O', 'qcow2',
                         '-c', extended_blobstore.name, qcow_path])

  # Write out a hash of the original blobstore file, so that subsequent runs
  # can trivially check if a cached extended FVM volume is available for reuse.
  with open(blobstore_hash_path, 'w') as blobstore_hash_file:
    blobstore_hash_file.write(current_blobstore_hash)

  return qcow_path


