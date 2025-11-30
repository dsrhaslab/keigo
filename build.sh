PROJECT_SOURCE_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJECT_BUILD_DIR="${PROJECT_SOURCE_DIR}/build"
PMDK_DIR="${PROJECT_SOURCE_DIR}/pmdk"
TBB_DIR="${PROJECT_SOURCE_DIR}/oneTBB"
YAML_DIR="${PROJECT_SOURCE_DIR}/yaml-cpp"
ROCKSDB_DIR="${PROJECT_SOURCE_DIR}/tiered-rocksdb"

BUILT_DEPS="${PROJECT_SOURCE_DIR}/.build_deps"
BUILT_PMDK="${BUILT_DEPS}/.built_pmdk"
BUILT_TBB="${BUILT_DEPS}/.built_tbb"
BUILT_YAML="${BUILT_DEPS}/.built_yaml"
BUILT_ROCKSDB="${BUILT_DEPS}/.built_rocksdb"


function build-tbb {
  # sudo cp -r ${PROJECT_SOURCE_DIR}/oneTBB/include/oneapi /usr/local/include/oneapi

  echo "=== build-tbb ===="
  cd ${TBB_DIR}
  mkdir build 
  cd build 
  cmake -DCMAKE_INSTALL_PREFIX="${TBB_DIR}/opt/TBB" -DCMAKE_BUILD_TYPE=Release ..
  make -j $(nproc)
  make install
  sudo cp -r $PROJECT_SOURCE_DIR/oneTBB/include/oneapi /usr/local/include/oneapi
}

function build-yaml {
  echo "=== build-yaml ===="
  cd ${YAML_DIR}
  mkdir build 
  cd build 
  cmake ..
  sudo make install
}

function build-pmdk_ {
  echo "=== build-pdmk ==="
  cd ${PMDK_DIR}
  make -j $(nproc)
}

function build-keigo {
  echo "=== build-keigo ==="
  cd ${PROJECT_SOURCE_DIR}/build
  cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_PROFILER_3000=ON ..
  # cmake -DCMAKE_BUILD_TYPE=Release -DUSE_PROFILER_3000=ON ..

  cmake --build $PROJECT_BUILD_DIR -- -j $(nproc)
  # make
  sudo make install
}

function build-rocksdb {
  echo "=== build-rocksdb ==="
  ROCKSDB_BUILD_DIR="${ROCKSDB_DIR}/build"
  mkdir -p ${ROCKSDB_BUILD_DIR}
  # cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_RTTI=true -B ${ROCKSDB_BUILD_DIR} -S ${ROCKSDB_DIR}
  cmake -DCMAKE_BUILD_TYPE=Release -DUSE_RTTI=true -B ${ROCKSDB_BUILD_DIR} -S ${ROCKSDB_DIR}

  cmake --build ${ROCKSDB_BUILD_DIR} -- -j $(nproc)
}



function build {
  #if [[ ! -d ${BUILT_DEPS} ]]; then 
    cd ${PROJECT_SOURCE_DIR}
    git submodule update --init
    mkdir -p ${BUILT_DEPS}
  #fi

  if [[ ! -f "${BUILT_PMDK}" ]]; then 
    build-pmdk_ && touch "${BUILT_PMDK}"
  fi 

  if [[ ! -f "${BUILT_TBB}" ]]; then 
    build-tbb && touch "${BUILT_TBB}"
  fi 

  if [[ ! -f "${BUILT_YAML}" ]]; then 
    build-yaml && touch "${BUILT_YAML}"
  fi 

  build-keigo

}

function clean {
  rm -rf ${BUILT_DEPS}
}

"$@"