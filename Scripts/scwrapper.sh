#!/bin/bash
set -euo pipefail

# MIT License
#
# Copyright (c) 2026 Brandon Pfeifer
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

function install() {
    remount_fs
    remove_files
    install_files

    read -r -n 1 -s -p 'Press any key to reboot...'
    reboot -f
}

function uninstall() {
    remount_fs
    remove_files

    read -r -n 1 -s -p 'Press any key to reboot...'
    reboot -f
}

function remount_fs() {
    if [[ "$(findmnt -n -o OPTIONS /)" =~ ^ro|,ro,|,ro$|^ro$ ]] ; then
        mount -o remount,rw /
    fi
}

function remove_files() {
   # NOTE: 0.0.3a no longer installs 99-disable-sc.rules
   rm -vf /etc/init.d/S99scwrapper              \
          /etc/udev/rules.d/99-disable-sc.rules \
          /usr/bin/scwrapper
}

function install_files() {
    DOWNLOAD_URL=https://github.com/bnpfeife/scwrapper/releases/download/latest/scwrapper.mister.tar.gz
    CA_CERTS=/etc/ssl/certs/cacert.pem

    if [[ ! -f "${CA_CERTS}" ]] ; then
       echo 'Could not locate CA certificates.'
       read -r -p 'Continue with insecure download (y/n): ' option

        if [[ "${option}" == y ]] ; then
            curl --insecure -fL "${DOWNLOAD_URL}" | tar -xzvf - -C /
        else
            exit 1
        fi
    else
        curl --cacert "${CA_CERTS}" -fL "${DOWNLOAD_URL}" | tar -xzvf - -C /
    fi
}


cat <<'EOF' || true
░█▀▀░▀█▀░█▀▀░█▀█░█▄█░░░█▀▀░█▀█░█▀█░▀█▀░█▀▄░█▀█░█░░░█░░░█▀▀░█▀▄░░░█░█░█▀▄░█▀█░█▀█░█▀█░█▀▀░█▀▄░
░▀▀█░░█░░█▀▀░█▀█░█░█░░░█░░░█░█░█░█░░█░░█▀▄░█░█░█░░░█░░░█▀▀░█▀▄░░░█▄█░█▀▄░█▀█░█▀▀░█▀▀░█▀▀░█▀▄░
░▀▀▀░░▀░░▀▀▀░▀░▀░▀░▀░░░▀▀▀░▀▀▀░▀░▀░░▀░░▀░▀░▀▀▀░▀▀▀░▀▀▀░▀▀▀░▀░▀░░░▀░▀░▀░▀░▀░▀░▀░░░▀░░░▀▀▀░▀░▀░

Please select one of the following options:
  i) install
  u) uninstall
EOF

read -r -p '> ' option

case "${option}" in
    i) install   ;;
    u) uninstall ;;
    *) echo 'Unrecognized option...' ;;
esac
