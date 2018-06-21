#!/bin/bash
 
user=`whoami`
home=$HOME
 
if [ "$user" = "nobody" ] ; then
    echo Not creating SSH keys for user $user
elif [ `echo $home | wc -w` -ne 1 ] ; then
    echo cannot determine home directory of user $user
else
    if ! [ -d $home ] ; then
        echo cannot find home directory $home
    elif ! [ -w $home ]; then
        echo the home directory $home is not writtable
    else
        file=$home/.ssh/id_rsa
        type=rsa
        if [ ! -e $file ] ; then
            echo generating ssh file $file ...
            ssh-keygen -t $type -N '' -f $file
        fi
 
        id="`cat $home/.ssh/id_rsa.pub`"
        file=$home/.ssh/authorized_keys
        if ! grep "^$id\$" $file >/dev/null 2>&1 ; then
            echo adding id to ssh file $file
            echo $id >> $file
        fi
 
        file=$home/.ssh/config
        if ! grep 'StrictHostKeyChecking.*no' $file >/dev/null 2>&1 ; then
            echo adding StrictHostKeyChecking=no to ssh config file $file
            echo 'StrictHostKeyChecking no' >> $file
        fi
 
        chmod 600 $home/.ssh/authorized_keys
        chmod 600 $home/.ssh/config
    fi
fi
