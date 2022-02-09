# Use the Ubuntu 20.04 image
FROM ubuntu:22.04

# Avoid prompts during package installation
ARG DEBIAN_FRONTEND=noninteractive

# Update the apt package list
RUN apt-get update

# Install Python3, pip and build-essential
RUN apt-get install -y python3 python3-pip build-essential bc

# Copy the current directory contents into the container at /app
COPY . /app

# Set the working directory to /app
WORKDIR /app

# Install Python packages
RUN pip3 install numpy==1.26.3 matplotlib==3.8.2 scipy==1.12.0

# Define the command to run when the container starts
CMD ["bash"]

