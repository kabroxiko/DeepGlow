import { useState, useEffect, useRef } from 'preact/hooks';

export function LiveClock({ time }) {
  const [displayedTime, setDisplayedTime] = useState('--:--');
  const intervalRef = useRef(null);

  useEffect(() => {
    console.log('LiveClock received time prop:', time);
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    if (time === '--:--') {
      setDisplayedTime('--:--');
      return;
    }
    if (typeof time === 'string' && /^\d{2}:\d{2}(:\d{2})?$/.test(time)) {
      setDisplayedTime(time);
      return;
    }
    // Otherwise, show local time and update every second
    function update() {
      const now = new Date();
      setDisplayedTime(
        now.toLocaleTimeString([], {
          hour: '2-digit',
          minute: '2-digit',
          second: '2-digit',
        })
      );
    }
    update();
    intervalRef.current = setInterval(update, 1000);
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
    };
  }, [time]);

  return <span id="currentTime">{displayedTime}</span>;
}
