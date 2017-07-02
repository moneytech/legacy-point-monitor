base_image="xenial"
image_name="pointmon"

build() {
	# set the root password for login (this should not be stored in git, this
	# is just for demo interaction)
	run-cmd "sh -c 'echo \"root:password\" | chpasswd'"

	# enable networking
	run-cmd "systemctl enable systemd-networkd"
	run-cmd "systemctl enable systemd-resolved"

	# add build dependencies
	run-cmd "apt update"
	run-cmd "apt install -y build-essential gcc make ctags"

	# build the repo
	run-cmd "sh -c 'cd /point && make && make install && make clean && make purge'"

	# register the service
	run-cmd "cp /point/init/point-* /etc/systemd/system/"
	run-cmd "systemctl enable point-monitor"
	run-cmd "systemctl enable point-installer"

	# we no longer need any of the build resources
	run-cmd "apt remove -y build-essential gcc make ctags"
}
