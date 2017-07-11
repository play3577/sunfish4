#!/bin/bash -eu

source config/ec2_test

APT_PACKAGES="cmake g++ git gsl-bin ruby-dev ruby-gsl golang"

scp -i ${EC2_SSH_KEY} ${REMOTE_BACKUP_KEY} ubuntu@${EC2_HOST}:~/.ssh/id_rsa

ssh -i ${EC2_SSH_KEY} -t -t ubuntu@${EC2_HOST} <<EOF
	chmod 600 .ssh/id_rsa

	sudo apt-get update -y
	sudo apt-get install -y ${APT_PACKAGES}
	sudo gem install gsl
	sudo gem install rgl

	exit
EOF
