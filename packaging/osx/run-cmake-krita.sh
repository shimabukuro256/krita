export MACOSX_DEPLOYMENT_TARGET=10.11
export DEPS_INSTALL_PREFIX=/Users/boud/dev/deps
export PYTHONPATH=$DEPS_INSTALL_PREFIX/sip:$DEPS_INSTALL_PREFIX/lib/python3.5/site-packages:$DEPS_INSTALL_PREFIX/lib/python3.5
export PYTHONHOME=$DEPS_INSTALL_PREFIX
cmake ../krita \
    -DCMAKE_INSTALL_PREFIX=/Users/boud/dev/i \
    -DCMAKE_PREFIX_PATH=/Users/boud/dev/deps \
    -DDEFINE_NO_DEPRECATED=0 \
     -DBUILD_TESTING=OFF \
    -DFOUNDATION_BUILD=ON  \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUNDLE_INSTALL_DIR=$HOME/dev/i/bin  \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11 \
    -DHIDE_SAFE_ASSERTS=ON \
    -DBUILD_TESTING=FALSE \
    -DPYQT_SIP_DIR_OVERRIDE=$DEPS_INSTALL_PREFIX/share/sip/ \
    -DHAVE_MEMORY_LEAK_TRACKER=FALSE
