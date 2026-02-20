IMAGE_NAME ?= jetson-builder

.PHONY: build run clean delete-images

build:
	@echo "Building Docker image $(IMAGE_NAME)..."
	docker build --build-arg UID=$(shell id -u) --build-arg GID=$(shell id -g) -t $(IMAGE_NAME) .

run:
	docker run --gpus all -it --rm --hostname jetson $(IMAGE_NAME)

clean:
	docker rmi $(IMAGE_NAME) || true

delete-images:
	docker rmi $(docker images -q) || true
