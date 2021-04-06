# ------------------------------------------- atomInflux --------------------------------------
import pytz
import datetime
from influxdb import InfluxDBClient

class AtomInflux:

  def __init__(self, host, port, user, password, dbname, session="dev", runNo=1):
    # Create the InfluxDB object
    self.client = InfluxDBClient(host, port, user, password, dbname)
    self.session = session
    self.runNo = runNo
    print("Influx client connected to host: {}, port: {}, user: {}, db: {}, session: {}, runNo: {}" \
            .format(host, port, user, dbname, session, runNo))


  def logObd2(self, obd2Dict):
    #print("obd2Dict: {}".format(obd2Dict))

    json_body = [
    {
      "measurement": self.session,
          "tags": {
              "run": self.runNo,
              },
          "time": datetime.datetime.now(tz=pytz.UTC),
          "fields": {
              "rpm" : obd2Dict['rpm'], "speed" : obd2Dict['speed'], "coolantTemp" : obd2Dict['coolantTemp'],
              "throttle" : obd2Dict['throttle'],
              "load" : obd2Dict['load'],"maf" : obd2Dict['maf'],"iat" : obd2Dict['iat']
          }
      }
    ]

    # Write JSON to InfluxDB
    self.client.write_points(json_body)

