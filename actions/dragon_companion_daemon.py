#!/usr/bin/env python3
"""
Dragon Companion - Active Learning System
Runs alongside the C dragon AI to provide enhanced learning, pattern recognition,
and proactive companion behaviors.
"""

import json
import time
import requests
import sqlite3
import os
import sys
from pathlib import Path
from collections import defaultdict
from datetime import datetime
import threading

class DragonCompanion:
    def __init__(self, api_host="localhost", api_port=8080):
        self.api_url = f"http://{api_host}:{api_port}"
        self.memory_db = Path("/tmp/dragon_memory.db")
        self.learning_cycles = 0
        self.patterns = defaultdict(lambda: {"count": 0, "reward": 0.0, "confidence": 0.0})
        self.insights = []
        self.last_state = {}
        self.cumulative_reward = 0.0
        self.cumulative_entropy = 0.0
        
        self._init_database()
        self._load_memory()

    def _init_database(self):
        """Initialize SQLite memory database"""
        try:
            conn = sqlite3.connect(str(self.memory_db))
            cursor = conn.cursor()
            
            # Create tables if not exist
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS memories (
                    id INTEGER PRIMARY KEY,
                    timestamp INTEGER,
                    event TEXT,
                    reward REAL,
                    pattern_name TEXT
                )
            ''')
            
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS patterns (
                    pattern_name TEXT PRIMARY KEY,
                    occurrences INTEGER,
                    total_reward REAL,
                    confidence REAL,
                    last_seen INTEGER
                )
            ''')
            
            conn.commit()
            conn.close()
        except Exception as e:
            print(f"Database init error: {e}")

    def _load_memory(self):
        """Load patterns from persistent storage"""
        try:
            conn = sqlite3.connect(str(self.memory_db))
            cursor = conn.cursor()
            cursor.execute('SELECT pattern_name, occurrences, total_reward, confidence FROM patterns')
            
            for pattern_name, occurrences, total_reward, confidence in cursor.fetchall():
                self.patterns[pattern_name] = {
                    "count": occurrences,
                    "reward": total_reward,
                    "confidence": confidence
                }
            
            conn.close()
        except Exception as e:
            print(f"Memory load error: {e}")

    def _save_memory(self):
        """Save patterns to persistent storage"""
        try:
            conn = sqlite3.connect(str(self.memory_db))
            cursor = conn.cursor()
            
            for pattern_name, data in self.patterns.items():
                cursor.execute('''
                    INSERT OR REPLACE INTO patterns 
                    (pattern_name, occurrences, total_reward, confidence, last_seen)
                    VALUES (?, ?, ?, ?, ?)
                ''', (pattern_name, data["count"], data["reward"], data["confidence"], int(time.time())))
            
            conn.commit()
            conn.close()
        except Exception as e:
            print(f"Memory save error: {e}")

    def get_status(self):
        """Fetch current dragon status"""
        try:
            response = requests.get(f"{self.api_url}/api/status", timeout=2)
            if response.status_code == 200:
                return response.json()
        except:
            pass
        return None

    def get_credits(self):
        """Fetch current credits"""
        try:
            response = requests.get(f"{self.api_url}/api/credits", timeout=2)
            if response.status_code == 200:
                return response.json().get("credits", 0.0)
        except:
            pass
        return 0.0

    def update(self):
        """Main learning cycle"""
        self.learning_cycles += 1
        status = self.get_status()
        
        if not status:
            return
        
        dragon = status.get("dragon", {})
        
        # Track entropy and reward
        entropy = dragon.get("policy_entropy", 0.0)
        reward = dragon.get("reward_total", 0.0)
        action = dragon.get("action", "idle")
        mood = dragon.get("mood", "calm")
        
        self.cumulative_entropy += entropy
        self.cumulative_reward = reward
        
        # Analyze event for patterns
        network_phase = status.get("network_phase_ms", 0)
        nearby_count = status.get("nearby_network_count", 0)
        
        self._analyze_event(action, mood, nearby_count, entropy)
        
        # Generate insights periodically
        if self.learning_cycles % 10 == 0:
            self._generate_insights()
            self._save_memory()
        
        return {
            "cycles": self.learning_cycles,
            "patterns_learned": len(self.patterns),
            "confidence": self._get_confidence(),
            "insights": self.insights[-3:] if self.insights else []
        }

    def _analyze_event(self, action, mood, nearby_count, entropy):
        """Analyze event and update patterns"""
        
        # Create event signature
        if action == "explore" and nearby_count > 0:
            pattern_name = f"explore_discovers_{nearby_count}_networks"
            self._record_pattern(pattern_name, entropy * 0.3)
        
        if entropy > 0.5:
            pattern_name = f"high_entropy_event_{mood}"
            self._record_pattern(pattern_name, entropy * 0.2)
        
        if action == "observe" and mood == "curious":
            pattern_name = "curious_observation_successful"
            self._record_pattern(pattern_name, 0.15)
        
        # Network discovery tracking
        if nearby_count > 5:
            pattern_name = f"rich_network_environment_{nearby_count}_networks"
            self._record_pattern(pattern_name, entropy * 0.25)

    def _record_pattern(self, pattern_name, reward):
        """Record a pattern occurrence"""
        if pattern_name not in self.patterns:
            self.patterns[pattern_name] = {
                "count": 0,
                "reward": 0.0,
                "confidence": 0.0
            }
        
        data = self.patterns[pattern_name]
        data["count"] += 1
        data["reward"] += reward
        
        # Calculate confidence (reward per occurrence)
        if data["count"] > 0:
            data["confidence"] = min((data["reward"] / data["count"]) * data["count"], 1.0)

    def _generate_insights(self):
        """Generate learning insights"""
        self.insights = []
        
        # Sort patterns by confidence
        sorted_patterns = sorted(
            self.patterns.items(),
            key=lambda x: x[1]["confidence"],
            reverse=True
        )
        
        if self.learning_cycles < 20:
            self.insights.append(
                f"🐉 Still learning! {self.learning_cycles} cycles complete. "
                f"Discovered {len(self.patterns)} patterns so far."
            )
        elif self.learning_cycles < 100:
            self.insights.append(
                f"🐉 Learning progress: {self.learning_cycles} cycles, "
                f"{len(self.patterns)} patterns. Confidence: {self._get_confidence()*100:.0f}%"
            )
        else:
            self.insights.append(
                f"🐉 Expert mode! {self.learning_cycles} learning cycles complete. "
                f"Managing {len(self.patterns)} behavioral patterns with "
                f"{self._get_confidence()*100:.0f}% confidence."
            )
        
        # Add top pattern insight
        if sorted_patterns:
            top_pattern, top_data = sorted_patterns[0]
            if top_data["confidence"] > 0.3:
                self.insights.append(
                    f"📊 Best Pattern: {top_pattern} "
                    f"(seen {top_data['count']}x, confidence: {top_data['confidence']*100:.0f}%)"
                )
        
        # Add learning encouragement
        if self.learning_cycles % 50 == 0:
            entropy_avg = self.cumulative_entropy / max(self.learning_cycles, 1)
            self.insights.append(
                f"💡 I'm discovering new network behaviors constantly! "
                f"Average entropy: {entropy_avg:.3f}"
            )

    def _get_confidence(self):
        """Calculate overall learning confidence"""
        if not self.patterns:
            return 0.0
        
        confidences = [p["confidence"] for p in self.patterns.values()]
        avg_confidence = sum(confidences) / len(confidences)
        
        # Factor in learning cycles
        cycle_factor = min(self.learning_cycles / 200.0, 1.0)
        
        return min(avg_confidence * 0.7 + cycle_factor * 0.3, 1.0)

    def log_state(self):
        """Log current companion state"""
        confidence = self._get_confidence()
        print(f"\n[Dragon Companion] Learning Cycle: {self.learning_cycles}")
        print(f"  Patterns Discovered: {len(self.patterns)}")
        print(f"  Overall Confidence: {confidence*100:.1f}%")
        print(f"  Recent Insights: {len(self.insights)}")
        
        for insight in self.insights[-2:]:
            print(f"    • {insight}")

def run_companion_daemon(interval=2.0):
    """Run companion as daemon process"""
    companion = DragonCompanion()
    
    print("🐉 Dragon Companion Active Learning System Started")
    print(f"   Learning every {interval} seconds")
    print(f"   Memory database: {companion.memory_db}")
    print("   Press Ctrl+C to stop\n")
    
    try:
        while True:
            result = companion.update()
            if result and result["cycles"] % 50 == 0:
                companion.log_state()
            time.sleep(interval)
    except KeyboardInterrupt:
        companion.log_state()
        print("\n🐉 Dragon Companion stopping... (memories saved)")
        companion._save_memory()

if __name__ == "__main__":
    # Accept command-line options
    interval = 2.0
    if len(sys.argv) > 1:
        if sys.argv[1] == "--fast":
            interval = 1.0
        elif sys.argv[1] == "--aggressive":
            interval = 0.5
    
    run_companion_daemon(interval)
