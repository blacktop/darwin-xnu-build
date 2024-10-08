MACOS_VERSION=sequoia-xcode
MACOS_VM_NAME=sequoia-codeql

.PHONY: deps
deps:
	@echo " > Installing dependencies"
	brew install hashicorp/tap/packer
	brew install cirruslabs/cli/tart
	brew install cirruslabs/cli/cirrus

.PHONY: build-vm
build-vm:
	@echo " > Building macOS VM"
	@packer init -upgrade ./templates/codeql.pkr.hcl
	@packer build -var "macos_version=$(MACOS_VERSION)" -var "macos_vm_name=$(MACOS_VM_NAME)" ./templates/codeql.pkr.hcl
	@echo " 🎉 Done! 🎉"

.PHONY: export-vm
export-vm:
	@echo " > EXPORTING macOS VM: $(MACOS_VM_NAME)"
	@tart export $(MACOS_VM_NAME)
	@echo " 🎉 Done! 🎉"

.PHONY: codeql-db
codeql-db:
	@echo " > Building CodeQL Database"
	@cirrus run
	@echo " 🎉 Done! 🎉"
	@cirrus run --artifacts-dir artifacts

clean:
	@echo " > Cleaning up"
	@rm -rf ./artifacts
	@rm -rf ./venv
	@rm -rf ./xnu-codeql
	@rm xnu-codeql.zip
	@echo " 🎉 Done! 🎉"

.DEFAULT_GOAL := build-vm