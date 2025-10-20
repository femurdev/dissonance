# Dissonance

**Dissonance** is a network-controlled bell system that lets users trigger real bells through a web interface. It connects a web app to physical bell controllers via wireless communication, enabling musical or alerting patterns to be played remotely.

## Features
- 🛜 Control physical bells wirelessly from any browser  
- 🎵 Trigger individual notes or full sequences  
- 🌐 Simple web interface for live interaction  
- ⚙️ Modular hardware and software design  
- 🧾 Bill of Materials (see `BOM.csv`) for easy replication  

## Architecture
1. **Frontend Web Interface**
   - Built with standard web technologies.
   - Sends bell commands via a backend BLE API.
2. **Backend Bells**
   - Receives user actions over BLE.
   - Sends control messages to all bell nodes.
   - See image below:
![Bell Connections](https://github.com/femurdev/dissonance/blob/main/softwareInteractionsv1.png?raw=true)
3. **Bell Nodes (Hardware)**
   - Each node controls a physical bell and actuator.
   - Uses BLE protocol.
   - Receives and executes play commands.

## Setup
1. Clone the repository:
   ```bash
   git clone https://github.com/femurdev/dissonance.git
   cd dissonance
   ```
2. Review hardware components in `BOM.csv`.
3. Flash firmware to each bell node (see `firmware/` if available).
4. Start the backend server (instructions TBD).
5. Open the web UI in your browser and start ringing bells!

## Project Status
> Currently under early development.  
> See [`JOURNAL.md`](JOURNAL.md) for design notes, progress, and experiments.

## License
This project is released under the [CC0-1.0 License](LICENSE), meaning it is public domain.
