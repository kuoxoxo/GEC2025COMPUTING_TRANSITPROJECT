# GEC2025 Transit Route Finder

**A web-based GTFS transit route finder with interactive mapping and real-time distance/time calculations.**

Project made by Dai Wei She, John McLean, Noah Harris'loe


## Overview

This application is a web-based transit route finder that loads GTFS (General Transit Feed Specification) data and allows users to find routes between two transit stops. It displays the route on an interactive map with detailed information about stops, distances, and estimated travel times.

## Features

**Interactive Map**
- Real-time latitude/longitude display as you move your mouse over the map
- Mapping made with OpenStreetMap tiles
- Interactive stop markers with detailed information when a path is generated

**Route Finding**
- Search stops by ID or exact name (case-insensitive matching)
- Finds routes between two stops that pass through all intermediate stops
- Shows all stops on the chosen route with sequential numbering

 **Visual Route Display (BETA)**
- Blue polyline showing the main route path
- Color-coded markers: green (start), red (end), blue (intermediate)

**Route Analytics**
- Estimates distance and travel time based on average transit speed (20 km/h)
- Displays results in the terminal interface

**Terminal Interface**
- Interactive command-line style interface
- Start/Restart buttons to begin or reset route searches
- Clean, minimal display of results

## Installation

### Prerequisites
- A modern web browser (Chrome, Firefox, Safari, Edge)

### Setup
To access the web version, visit
https://kuoxoxo.github.io/GEC2025COMPUTING_TRANSITPROJECT/


## Usage

### Map

You can move around the map of Guelph, or even the whole world! (You can also discover the Earth is flat)

### Starting the Program

1. Click the **"Start"** button to begin
2. The program will load transit data (stops, trips, and stop times)
3. You'll be prompted to enter an origin stop

### Finding a Route

1. **Enter Origin Stop:**
   - Type a stop ID (e.g., `105`) or exact stop name (e.g., `Gordon at Kortright southbound`)
   - Press Enter or click Submit
   - The system will find and display the stop

2. **Enter Final Stop:**
   - Type a stop ID or exact stop name for your destination
   - Press Enter or click Submit
   - The system will find a route connecting both stops

3. **View Results:**
   - The map displays the complete route with markers
   - Terminal shows:
     - Number of stops in the route
     - Total distance in kilometers
     - Estimated travel time

### Restarting

Click the **"Restart"** button to clear the map and start a new search.
