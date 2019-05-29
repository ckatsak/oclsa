FROM ubuntu:18.04 AS build

ENV CLANG_HOME /clang+llvm-7.0.1-x86_64-linux-gnu-ubuntu-18.04
ENV LLVM_DIR $CLANG_HOME/lib/cmake/llvm
RUN apt-get update && apt-get install -y --no-install-recommends \
		curl git xz-utils build-essential cmake python3-pip \
	&& rm -rf /var/lib/apt/lists/* \
	&& curl -SL https://releases.llvm.org/7.0.1/clang+llvm-7.0.1-x86_64-linux-gnu-ubuntu-18.04.tar.xz | tar -xJC / \
	&& git clone --branch master --single-branch https://github.com/llvm-mirror/libclc.git /libclc \
	&& git clone --branch demofeatxtr --single-branch https://github.com/ckatsak/oclsa.git /oclsa \
	&& mkdir /oclsa/_build && cd /oclsa/_build && cmake .. && make


FROM ubuntu:18.04
LABEL maintainer "ckatsak@gmail.com"

COPY --from=build /clang+llvm-7.0.1-x86_64-linux-gnu-ubuntu-18.04 /clang+llvm-7.0.1-x86_64-linux-gnu-ubuntu-18.04/
COPY --from=build /libclc /libclc/
COPY --from=build /oclsa/_build/oclsa/libLLVMoclsa.so /usr/lib/libLLVMoclsa.so
COPY --from=build /oclsa/demofeatxtr/ /demofeatxtr/

RUN apt-get update && apt-get install -y --no-install-recommends python3-pip \
	&& rm -rf /var/lib/apt/lists/* \
	&& pip3 install -r /demofeatxtr/requirements.txt

ENV CLANG_HOME /clang+llvm-7.0.1-x86_64-linux-gnu-ubuntu-18.04
ENV PATH $CLANG_HOME/bin:$PATH
ENV LIBCLC_HOME /libclc
EXPOSE 54242

ENTRYPOINT [ "python3", "/demofeatxtr/demofeatxtr.py" ]

# $ docker build --no-cache -t ckatsak/demofeatxtr:v0.1.0 .
# $ docker run --rm -d -p 54242:54242 ckatsak/demofeatxtr:v0.1.0
