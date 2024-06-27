packer {
  required_plugins {
    tart = {
      version = ">= 1.2.0"
      source  = "github.com/cirruslabs/tart"
    }
  }
}

variable "macos_version" {
  type = string
}

variable "macos_vm_name" {
  type = string
}

source "tart-cli" "tart" {
  vm_base_name = "ghcr.io/cirruslabs/macos-${var.macos_version}:latest"
  vm_name      = "${var.macos_vm_name}"
  cpu_count    = 4
  memory_gb    = 8
  disk_size_gb = 90
  headless     = true
  ssh_password = "admin"
  ssh_username = "admin"
  ssh_timeout  = "120s"
}

build {
  sources = ["source.tart-cli.tart"]

  provisioner "shell" {
    inline = [
      "source ~/.zprofile",
      "brew --version",
      "brew update",
      "brew upgrade",
      "brew install jq gum cmake ninja",
    ]
  }

  provisioner "shell" {
    inline = [
      "source ~/.zprofile",
      "brew --version",
      "brew update",
      "brew upgrade",
      "brew install codeql",
      "codeql pack download codeql/cpp-queries",
    ]
  }

  // `ipsw` development tools
  provisioner "shell" {
    inline = [
      "source ~/.zprofile",
      "brew --version",
      "brew update",
      "brew upgrade",
      "brew install go goreleaser zig unicorn libusb",
      "brew install blacktop/tap/ipsw",
      "go install golang.org/x/tools/...@latest",
      "go install github.com/spf13/cobra-cli@latest",
      "go get -d golang.org/x/tools/cmd/cover",
      "go get -d golang.org/x/tools/cmd/stringer",
      "go install github.com/caarlos0/svu@v1.4.1",
    ]
  }

}
