#!/usr/bin/env bash
set -ue # errors and undefined vars are fatal

source ./build-steps.sh

# Make sure only root can run our script
[ $(whoami) = root ] || exec sudo $0 $*

RED=$(tput setaf 1)
NORMAL=$(tput sgr0)
BOLD=$(tput bold)
YELLOW=$(tput setaf 3)
GREEN=$(tput setaf 2)

has_failed=0

random-string() {
  cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w ${1:-10} | head -n 1
}

run-cmd() {
  printf "   %-75s %-s\r"                  "$1 "  "[ ${YELLOW}running${NORMAL} ]"
  set +e
  $MACHINE_CONFIG bash -c "$1 ; exit" &>/dev/null
  cmd_result=$?
  set -e
  if [ $cmd_result -eq 0 ]; then
    printf "   %-75s %-s\n"                  "$1 "  "[ done ]   "
  else
    printf "   ${BOLD}%-75s${NORMAL} %-s\n"  "$1 "  "[ ${RED}${BOLD}failed${NORMAL} ] "
    has_failed=1
  fi
}

# don't leave build containers around upon failed builds
function exit_with_error {
  set +e
	echo "${BOLD}${RED}Image build failed!${NORMAL}"
  # TODO: stop and rm?
}

MACHINE_NAME_TEMP=$(random-string)
MACHINE_CONFIG="systemd-nspawn -q -M $MACHINE_NAME_TEMP -D $image_name/ --bind=$(pwd):/point  --bind-ro=/run/resolvconf/resolv.conf:/run/resolvconf/resolv.conf"

trap exit_with_error EXIT

echo "${BOLD}Downloading OS image${NORMAL} (could take a while)"
set +e
debootstrap $base_image $image_name http://archive.ubuntu.com/ubuntu/ > /dev/null
set -e

echo "${BOLD}Creating build container${NORMAL}"
build

if [ $has_failed -ne 0 ]; then
  exit 1
fi

# create on archive of the container dir
echo "${BOLD}Creating image archive${NORMAL}"
archive_name=$image_name.tar.gz
tar -czf $archive_name ./$image_name/

# graceful exit
trap - EXIT

# TODO: check for failed
echo "${BOLD}Image '$archive_name' created successfully!${NORMAL}"
