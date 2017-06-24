#!/usr/bin/env bash
set -ue # errors and undefined vars are fatal

base_image="images:alpine/3.5"
image_name="pointmon"
# this shows up in the description column for 'lxc image list'
image_description="${image_name}: runs install_data & monitor_shm ($base_image)"

echo "Creating build container"
init_container=$(lxc init $base_image)
container=${init_container#*: }
lxc start $container
sleep 5 # this should be enough time for the container to fully start

# don't leave build containers around upon failed builds
function exit_with_error {
  set +e
	echo "Image build failed!"
  lxc stop $container --timeout 3 --force
  lxc rm   $container --force
}

trap exit_with_error EXIT

# show steps along the way
set -x

# add build dependencies
# note: we are not keeping around the apk index and we are grouping these packages into a group to reference later (for removal)
lxc exec $container -- apk add --no-cache  --virtual .build-deps build-base gcc make ctags

# provision the container with the current (local) repo and build
lxc exec $container -- mkdir /point
lxc file push -r       . $container/point/
lxc exec $container -- sh -c "cd /point && make && make install"

# register the service
lxc file push          init/point* $container/etc/init.d/
lxc exec $container -- chmod 755 /etc/init.d/point-installer /etc/init.d/point-monitor
lxc exec $container -- chown root:root /etc/init.d/point-installer /etc/init.d/point-monitor
lxc exec $container -- chown -R nobody:nobody /point
lxc exec $container -- ln -s /etc/init.d/point-monitor /etc/runlevels/default/point-monitor
lxc exec $container -- ln -s /etc/init.d/point-installer /etc/runlevels/default/point-installer

# we no longer need any of the build resources or the local src repo
lxc exec $container -- rm -rf /point
lxc exec $container -- apk del .build-deps

# promote the current container to an image and remove the container. The --force
# option will stop the container before publishing.
lxc publish $container --force --alias=$image_name description="$image_description"
lxc rm $container --force

# graceful exit
set +x
trap - EXIT
echo "Image '$image_name' created successfully!"
